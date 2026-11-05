# Super Dragon Ball Z — Physics & Collision Research Report

## Source scope
Research performed in `C:\Dev\SuperDragonBallZ\.kilo\worktrees\physics-collision`.
Key sources: `source/core/stage_collision.cpp/.hpp`, `source/gameplay/player_controller.cpp/.hpp`,
`source/gameplay/player_spawn.cpp/.hpp`, `source/gameplay/player_animation.cpp`,
`source/gameplay/camera.cpp/.hpp`, `source/core/stage.cpp/.hpp`, `source/core/math.cpp/.hpp`,
`source/render/stage_renderer.cpp`, `tests/test_stage_collision.cpp`,
`tests/test_player_controller.cpp`, `tests/test_player_spawn.cpp`.

## 1. HIT object / polygon / vertex format

Binary layout (resource-local, little-endian), confirmed by `parse_hit` in
`source/core/stage.cpp`:

- Header: 32 bytes. Magic `HIT ` at +4, version `0x00020000` at +0. Object count (u32) at
  +8, objects offset at +12, polygons offset at +16, vertices offset at +20.
- Names table: `object_count` × 16-byte fixed CP932 names starting at offset 32.
- Objects: `object_count` × 64-byte records at `objects_offset`. Each object: flags (u32
  @+0), polygon count (u32 @+4), first polygon (u32 @+8), radius (f32 @+12), three
  float4 vectors (48 bytes @+16..+64). Object name is the matching entry from the names
  table.
- Polygons: 32 bytes each at `polygons_offset`. Each: vertex count (u32 @+0), first
  vertex (u32 @+4), two unknown words (u32 @+8, +12), float4 normal (16 bytes @+16..+32).
- Vertices: float4 (16 bytes each: x,y,z,w) at `vertices_offset`. Polygon vertex ranges
  are contiguous and non-overlapping (`first_vertex` must equal the running total; no
  unreferenced vertex tail). Object polygon ranges may overlap intentionally.

In-memory structs (`source/core/stage.hpp`):
- `s_hit_vertex`: `std::array<float,4> m_position`
- `s_hit_polygon`: `m_vertex_count`, `m_first_vertex`, `m_unknown_words[2]`, `m_normal[4]`
- `s_hit_object`: `m_name`, `m_flags`, `m_polygon_count`, `m_first_polygon`, `m_radius`,
  `m_vectors[3][4]`
- `s_hit`: `m_names`, `m_objects`, `m_polygons`, `m_vertices`

All 13 checked STG/HIT pairs parse: 2,886 named HIT objects, 20,206 polygons, 66,301
vertices. Unknown STG payload words and HIT object flags/vectors are retained without
guessed semantics. Native `0x1ABAB0` binds header offsets to names/objects/polygons/
vertices; `0x1AB840` searches 16-byte object names and indexes object records.

## 2. Newell's method normals

`source/core/stage_collision.cpp:30-63` (`polygon_normal`) and `stage_collision.cpp:
628-637` (full 3D area).

- Computed from **transformed world-space vertices** after applying each STG node's world
  matrix — normals are never transformed like positions.
- Newell's summation over consecutive vertex pairs `(current, next)`, wrapping
  `(index+1) % count`, works for arbitrary planar n-gons and does not assume the first
  three points are non-collinear.
- Result is normalised; then flipped so `normal[1] >= 0` — collision normals are always
  upward-oriented regardless of source polygon winding.
- Degenerate polygons (fewer than 3 vertices, or Newell length ≤ epsilon) are counted in
  `m_degenerate_polygons` and skipped.
- Full 3D area = `|Newell vector| * 0.5f` stored in `m_area`; `m_projected_area` is the XZ
  shoelace area (used for spawn ranking).
- Point-in-polygon tests (`point_in_polygon_xz`, ray casting in XZ) replace any
  triangle-fan assumption, so concave n-gons are queried as polygons.

Tests confirm: rotated + non-uniformly scaled HIT surfaces get correctly recomputed
normals (tilted test: normal `(0, 0.866, 0.5)`), and a concave n-gon's notch is correctly
rejected as floor.

## 3. 8-unit XZ spatial grid broad-phase

`source/core/stage_collision.cpp:441-497` (`build_stage_collision_spatial_index`), queried
via `collect_collision_candidates` (`stage_collision.cpp:181-239`).

- Cell size `m_spatial_cell_size = 8.0f` (configurable). Cells keyed by
  `(cell_x << 32) | cell_z` in an `unordered_map<uint64_t, vector<polygon_index>>`.
- Each polygon is inserted into **every cell** touched by its world-space XZ bounds
  (`floor(value / 8)`). Cell coordinates use double-precision division with int32
  clamping/overflow guards.
- `m_spatial_index_ready` flag; `build_stage_collision_world()` builds it automatically.
  Hand-modified worlds must call `build_stage_collision_spatial_index()` again.
- **Large-polygon fallback**: polygons spanning more than 4,096 cells (width, depth, or
  width×depth) go into `m_large_polygon_indices` instead of the grid, so arena-sized
  floors don't explode the grid. The 200×200-scaled floor polygon lands in the fallback
  list and remains queryable.
- Query cell budget: max 8,192 cells total per query (width, depth, or product).
  Exceeding it falls back to a full linear scan — also used when the index isn't ready.
- Candidates are de-duplicated (sort + unique) because a polygon can appear in multiple
  cells.
- Indexed vs linear query equivalence is tested over 36 separated collision instances.

## 4. Capsule support queries (centreline + footprint ring sampling)

### `query_stage_ground_capsule` (`stage_collision.cpp:681-805`)

1. **Centreline query**: calls `query_stage_ground` at the capsule centre `(x, z)`. This
   is the reference contact — the capsule's lowest point is a point at its centreline,
   not a flat disk.
2. **Full-footprint validation**: `is_stage_capsule_supported` checks both rings over the
   complete collision world. Returns the centre contact only if the footprint is
   supported.
3. **Narrow-surface exception**: if the supporting HIT polygon's smaller XZ extent ≤
   `2*radius + 0.01` and the centre has `≥ 0.45*radius` interior clearance, the centre
   contact is accepted even though a full disk would overhang (covers rails/strips).
4. **Seam fallback**: if the centre finds no direct polygon, 16 ring samples at
   `0.85*radius` are queried with expanded step/drop (`+ radius`). Each sample's height
   is projected back to the centre along the sample polygon's normal. A **majority (≥ 8
   of 16)** must agree on a consistent centre height (within
   `max(0.12, radius*0.5)` tolerance); the median height wins. This bridges tiny
   inter-polygon cracks but cannot keep a player grounded across an open gap (a single
   edge polygon can't manufacture support).

### `is_stage_capsule_supported` (`stage_collision.cpp:983-1049`)

- Confirms centre contact within `surface_tolerance`, then samples **two rings**: 12
  inner samples at `0.5*radius`, 24 outer samples at `0.98*radius`.
- Each sample expects a height derived from the centre plane:
  `expected_y = surface_y - (nx*ox + nz*oz)/ny`.
- Per-sample tolerance = `max(tolerance, ring_radius * tan(max_slope))`, so walkable
  ramps meeting flat floors at a crease are allowed (height variation implied by slope
  over the radius). No fixed normal-dot cutoff between adjacent polygons — only each
  sample must resolve to a walkable surface.
- Every sample must find walkable ground, preserving ledge detection.

### Regression notes (from BUILD_STATUS.md)

- Earlier code required every point around the radius to overlap floor (flat-bottom
  cylinder model) — dropped contact on narrow strips/seams. Fixed by preferring the
  centreline contact with constrained fallbacks.
- Earlier code used the highest ring point as support height — lifted the player on
  slopes and kept support when only one side remained over a ledge. Fixed: centre contact
  height returned, footprint validated separately.

## 5. Wall blocking / sliding, overhead clearance, edge falls

### Wall blocking & sliding — `resolve_stage_horizontal_movement`
(`stage_collision.cpp:1051-1142`)

- Near-vertical HIT polygons (`normal[1] < cos(max_slope)`) with vertical overlap of the
  capsule's feet..feet+height are treated as blocking faces.
- Swept test: signed distance of start and target capsule centre to the wall plane (along
  horizontal normal `nx, nz`), with a `side` sign. If `signed_end >= radius` or moving
  away (`signed_end >= signed_start`), skip.
- Contact fraction = `(signed_start - radius) / (signed_start - signed_end)`, clamped
  `[0,1]`. Tangent extent check: contact point's tangent coordinate must lie within the
  polygon's vertex tangent range ± radius (finite wall extents).
- **Sliding**: only the penetrating normal component is removed:
  `resolved += nx * side * (radius - signed_end + 0.001f)`. Tangential movement is
  preserved, so the player slides along walls. Small `0.001` slop avoids repeated
  floating-point contact.
- Vertical overlap uses the wall's vertical bounds to pick the cross-section radius
  (`radius_at_y` at wall mid-height) — low barriers hitting only the bottom/top
  hemisphere are not tested against the full cylinder radius (verified by `low_barrier`
  test).

### Capsule clearance — `is_stage_capsule_clear` / `capsule_clear_impl`
(`stage_collision.cpp:326-438`, `972-981`)

- Models the capsule as bottom hemisphere + cylinder + top hemisphere.
  `cap_radius = min(radius, height*0.5)`; if height ≤ diameter, treated as a sphere.
- `radius_at_y(y)` returns the cross-section radius at each height (hemisphere curvature
  near caps, `cap_radius` in the cylinder band).
- Walkable (upward) polygons: only reject if the surface height at `(x,z)` lies strictly
  inside the capsule volume (`feet_y + 0.08 < surface_y < feet_y + height - 0.05`) —
  ignores the support surface at the player's feet, rejects other walkable faces
  (ceilings/overhead platforms).
- Near-vertical polygons: plane distance vs `effective_radius` at the wall's vertical
  overlap, plus tangent-extent test.

### Overhead clearance

In `update_collision_step` (`player_controller.cpp:158-188`): if `next_y > position.y`
(rising) and the destination capsule is not clear, check the **current column**. If the
current column is clear, hold horizontal movement for the tick (uphill momentum into a
ramp) while upward movement continues. If the current column is **not** clear, stop
upward movement: `next_y = position.y`, `velocity.y = 0` — a real overhead HIT surface
blocks the jump column (verified by `low_ceiling` test: jump stopped before entering the
surface).

### Edge falls

- Ground support is resolved **at the destination** (`next_x, next_z`) after horizontal
  movement, with `max_drop = step_down` when previously grounded, `1,000,000` when
  airborne. Walking off an edge → no support at destination → `m_grounded = false`,
  player falls (verified by `edge_controller` test: walks off HIT platform, falls below
  start height).
- Vertical crossing is still detected via previous/next feet heights.

## 6. Player controller

`source/gameplay/player_controller.hpp` defaults, `source/gameplay/player_controller.cpp`.

### Constants (tunable on `s_player_controller`)

| Field | Default | Meaning |
|---|---|---|
| `m_walk_speed` | 8.0 | units/s |
| `m_run_speed` | 14.0 | units/s (Shift / left shoulder) |
| `m_jump_speed` | 7.5 | units/s upward impulse |
| `m_gravity` | 28.0 | units/s² |
| `m_acceleration` | 36.0 | units/s² (with input) |
| `m_deceleration` | 42.0 | units/s² (no input) |
| `m_collision_capsule_radius` | 0.25 | |
| `m_collision_capsule_height` | 1.75 | |
| `m_collision_foot_offset` | 0.05 | feet above capsule base |
| `m_collision_step_height` | 0.42 | max step up per tick (~0.40 at run speed on 60° ramp) |
| `m_collision_step_down` | 0.65 | max drop while grounded |
| `m_max_walkable_slope_degrees` | 60.0 | near-vertical above this = wall |

### Fixed 60 Hz timestep

`update_player_controller` (`player_controller.cpp:256-296`): clamps `delta_seconds` to
`[0, 0.25]`, accumulates into `m_fixed_time_accumulator`, and simulates in
`k_fixed_step = 1/60` s substeps. Movement is independent of render-frame chunking (test:
one 0.1s update equals six 1/60s updates). Jump press edge is latched into
`m_jump_pressed_pending` and consumed at the next fixed tick — survives sub-frame render
updates but is **not** carried through a fall to trigger on landing (test: held jump
while falling is consumed, not re-triggered on landing).

### Camera-relative input — `make_camera_relative_player_input`
(`player_controller.cpp:238-248`)

Rotates local `(move_x, move_forward)` by camera yaw. Forward axis is negative Z. At 90°
camera yaw, forward input becomes pure world-X. The gameplay camera's signed orbit
distance can cross zero; the negative-orbit side adds 180° to the heading so controls
remain camera-relative (see camera.cpp / README).

### Ground support at destination

`update_collision_step` (`player_controller.cpp:102-235`):

1. If grounded, query support at current position
   (`query_stage_ground_capsule` at `previous_y - foot_offset`), snap `position.y` to
   surface + foot offset.
2. Apply input/jump, gravity if airborne.
3. Compute desired `next_x/next_z` from velocity, resolve horizontal movement against
   walls (`resolve_stage_horizontal_movement`).
4. Overhead check if rising.
5. **Resolve support at the destination** `(next_x, next_z)` with
   `support_reference_y = max(previous_y, next_y) - foot_offset`. Not a path-wide maximum
   — an intermediate point can't keep the player grounded after the final position left
   the floor, nor pull the player up early onto a platform.
6. Land conditions: walked onto surface (within step_down/step_height band, descending)
   or landed (crossing the support height with downward velocity). On land: snap
   `position.y = support_y`, zero vertical velocity, set ground normal/polygon/object,
   `m_grounded = true`, set `m_landed_this_frame`.

### Animation state — `source/gameplay/player_animation.cpp`

`classify_player_animation_state`: not grounded → jump (`vel.y > 0.15`) or fall; grounded
& slow → idle; grounded & fast → run if `m_running` else walk. PMT clips selected by name
tokens (idle/wait/stand/stnd, walk/move/step, run/dash/sprint, jump/jamp/takeoff,
fall/airborne/air, land/touchdown/touch) with fallbacks (run→walk→idle, walk→idle,
fall→jump, land→idle). Unrecognised names keep the current clip — no guessed indices.

## 7. HIT wireframe debug

`source/render/stage_renderer.cpp:295-330` (`render_stage_viewer`, `ctx.show_collision`):

- Draws every transformed HIT polygon as a `GL_LINES` loop of its world-space vertices,
  preserving n-gon boundaries (no guessed triangulation).
- Colour coding: orange `(1, 0.85, 0.05)` for the player's current ground polygon; green
  `(0.1, 1, 0.3)` for walkable (`normal[1] >= 0.6427876` ≈ cos 50°); red `(1, 0.2, 0.15)`
  for steep/wall.
- Toggled with **H** (transformed HIT polygons + player capsule diagnostics).
- Player capsule debug (`stage_renderer.cpp:332-414`): wireframe capsule using the clamped
  cap radius (`min(radius, height*0.5)`), bottom/top hemispheres (8 rings × 24 segments),
  4 vertical cylinder lines + centre lines, and a ground-normal vector (0.75 units).

## 8. Stage auto-framing from mesh bounds

`source/render/stage_renderer.cpp:124-175`:

- Runs once when `!framing_camera->m_framed` (after stage load). Targets the **viewer
  camera** (or active camera if no viewer camera); gameplay camera follow is explicitly
  never reset by stage auto-framing.
- Iterates kind-2 (mesh) nodes, transforms each mesh vertex by the node's world matrix,
  accumulates world-space min/max bounds.
- `m_target = centre of bounds`, `m_distance = max(extent * 0.6, 0.1)`, `m_pitch = -30°`,
  `m_yaw = -30°` (angled top-down view). `m_framed = true`.
- Prints `Stage bbox: (...) to (...) distance=...` via `std::println`.
- Bounds are from kind-2 mesh nodes only; collision polygons don't extend the framing
  bounds.

## 9. Ground-query edge cases

- **Narrow HIT strips / rails**: centreline contact + narrow-surface exception
  (`extent ≤ 2r + 0.01`, centre clearance ≥ 0.45r) keeps the player grounded where a
  full-disk check would fail. Test: 0.24-wide strip keeps controller grounded for 2
  seconds.
- **Tiny seams between polygons**: centre falls into the crack → seam fallback reconstructs
  height from majority of 16 ring samples on a consistent plane (test: 0.02-wide gap
  bridged). Wide-floor ledge rejection still applies (a single edge polygon can't hold
  support).
- **Ramp-to-flat crease**: no normal-dot cutoff between adjacent walkable polygons;
  per-sample tolerance allows slope-implied height variation. Test: capsule supported
  across a 45° crease; player crosses from ramp to flat floor and can jump at the crease.
- **Slopes up to 60°**: walkable limit raised from 50° to 60°; near-vertical faces remain
  walls. Step allowance 0.42 supports run speed at 60 Hz on a 60° ramp.
- **Ledge detection**: full outer ring (0.98r) must be supported; footprint hanging over
  an edge rejects support and the controller falls (test: at x=1.9 on a [-2,2] floor with
  r=0.25, capsule ground query rejects; controller falls).
- **Spawn height reference**: `m_stage_origin_y` from the live STAGE root's world-space Y,
  not absolute Y=0. Stages translated upward spawn on their own floor. Ranking: nearest
  height delta first, then interior clearance, then area — so a lower small floor beats a
  higher large platform, and below-stage geometry (e.g. world bounds at Y=-64) can't win
  just by being lower.
- **Spawn capsule fit**: spawn candidate rejected if the capsule footprint isn't
  supported or the volume isn't clear (thin strip test rejected).
- **Ground memory after movement**: support queried at destination, not path max — prevents
  "grounded from an intermediate point after leaving the floor" and "pulled up onto a
  platform before reaching it."

## 10. Spawn resolution vs collision queries — detailed interaction

### `find_stage_player_spawn` (`stage_collision.cpp:807-970`)

The spawn finder iterates every walkable polygon in the collision world and ranks candidate
interior points against the live STAGE root origin.

**Ranking criteria** (`s_rank` struct, `is_better` lambda):
1. `m_height_delta` — `abs(surface_y - m_stage_origin_y)`, ascending (nearest to STAGE root Y wins)
2. `m_clearance` — `distance_to_polygon_edge_xz` at the candidate point, descending (deepest interior)
3. `m_area` — `m_projected_area` (XZ shoelace area), descending (largest footprint)

This ordering means a lower, smaller floor beats a higher, larger platform purely on height delta;
among surfaces at similar height, deeper interior clearance wins; ties break on area. Below-stage
geometry (e.g. world-bounds plane at Y=-64) cannot win just by being lower — it is ranked by delta
from the STAGE root, so a surface at Y=0 with STAGE root at Y=0 beats one at Y=-64.

**Candidate generation per polygon**:
- Polygon interior centroid (computed via the polygon-area centroid formula, then verified
  `point_in_polygon_xz`); if the centroid falls outside (concave polygon), fall back to a 15×15
  bounded grid search picking the point with maximum edge clearance.
- Up to 48 points per polygon (`k_max_points_per_polygon`), sorted by height delta then clearance.
- Early-break: once a point on a polygon cannot beat the current best rank, later points on that
  same polygon are skipped (points are pre-sorted).

**Validation of each candidate point**:
- `is_stage_capsule_supported(world, x, z, surface_y, capsule_radius, max_slope_degrees)` — confirms
  centre contact within `surface_tolerance`, then samples two rings (12 inner at 0.5r, 24 outer at
  0.98r) over the complete collision world. Every sample must resolve to a walkable surface within
  the per-sample slope tolerance. Adjacent HIT triangles/quads can jointly support a spawn across
  their shared seam; no fixed normal-dot cutoff between walkable polygons.
- `is_stage_capsule_clear(world, x, z, surface_y, capsule_radius, capsule_height, max_slope_degrees)`
  — `capsule_clear_impl` rejects any walkable face whose height at (x,z) lies strictly inside the
  capsule volume (`feet_y + 0.08 < surface_y < feet_y + height - 0.05`), and rejects near-vertical
  faces intersecting the capsule cross-section (radius scaled to the wall's vertical overlap).

**Spawn result**: `{ x, surface_y + foot_offset, z }`, polygon normal, polygon/stage-node/hit-object
indices, projected area, object name. Foot offset (default 0.05) lifts feet slightly above the
surface.

### `resolve_player_spawn_override` (`source/gameplay/player_spawn.cpp:95-132`)

`player_spawns.cfg` entries are validated against the **transformed** HIT geometry, not against
raw resource-local coordinates:

- The override's XZ position is queried with `query_stage_ground(world, x, z, expected_surface_y,
  tolerance, tolerance, max_slope)` where `expected_surface_y = override_y - foot_offset` and
  tolerance = `vertical_tolerance` (default 2.0). The contact must exist and its polygon index must
  be valid.
- The polygon at the contact is checked with `is_stage_capsule_supported` (full footprint over the
  complete world) and `is_stage_capsule_clear` (capsule volume clear).
- If either check fails, the override is rejected and the automatic spawn finder is used instead.
- Matches only the primary stage name (case-insensitive), not arbitrary mesh/HIT strings elsewhere
  in the STG name table. Invalid rows (non-finite values, trailing tokens, missing fields) are
  ignored during parsing.

### Interaction summary

Spawn resolution depends on the same collision primitives used at runtime (centreline ground query,
ring footprint sampling, capsule clearance), so a spawn point that passes validation is guaranteed
to be a point where the player controller can stand. The centreline contact height (not a ring
maximum) is used, preventing spawn points from being lifted on slopes. The STAGE-root-relative height
reference keeps spawn height correct for translated stages, decoupled from absolute world Y=0.

## 11. Stage auto-framing vs spawn height reference

### Auto-framing (`source/render/stage_renderer.cpp:124-175`)

- Runs once per stage load when `!framing_camera->m_framed`. Targets the **viewer camera** (or active
  camera if no viewer camera); gameplay camera follow is explicitly never reset by auto-framing.
- Iterates kind-2 (mesh) nodes only, transforms each mesh vertex by the node's world matrix,
  accumulates world-space min/max bounds.
- `m_target = centre of bounds`, `m_distance = max(extent * 0.6, 0.1)`, `m_pitch = -30°`,
  `m_yaw = -30°` (angled top-down view). Prints `Stage bbox: (...) to (...) distance=...`.
- Bounds come from kind-2 mesh nodes only; collision polygons do **not** extend the framing bounds.

### Spawn height reference (`m_stage_origin_y`)

- Set in `build_stage_collision_world` (`stage_collision.cpp:521-523`) from the live STAGE root's
  world-space Y via `mat4_point(stage_worlds[stage.m_stage_root], {0,0,0})[1]`. Only kind-3 HIT
  nodes under the STAGE root enter the collision world; template and EFFECT subtrees are excluded
  (they sit near the origin and must not become spawn surfaces).
- `find_stage_player_spawn` uses `m_stage_origin_y` as `reference_y` for height-delta ranking.
- Spawn overrides are validated at their configured XZ against the transformed HIT geometry at the
  STAGE-root-relative height.

### Interaction

Auto-framing and spawn height reference both derive from world-space stage placement but serve
different purposes: auto-framing centres the viewer on authored mesh bounds, while `m_stage_origin_y`
anchors spawn selection to the STAGE root's world Y. A stage translated upward in world space will
have its viewer camera framed around the translated mesh bounds, and its spawn point resolved on the
translated floor — both consistent with the same world transform. The gameplay camera is independent
of both.

## Build / validation status

Native core builds and all five CTest targets pass (GCC 14.2 Release, Clang 17 Release,
ASan+UBSan Debug). The SDL3 runtime was **not** compiled here (SDL3 unavailable), so
in-game behavior and real-stage spawn quality remain unverified against the full regional
INFO.DAT/GAME.DAT corpus. No real stage has been batch-validated; synthetic tests
validate transformation/query code, not semantic correctness of every original stage's HIT
references. `player_spawns.cfg` provides validated per-stage spawn overrides for stages
where the automatic heuristic can't infer the authored start.

## Key files

- `source/core/stage_collision.cpp` — collision world build + all queries (ground,
  capsule support/clearance, horizontal movement, spawn finder)
- `source/gameplay/player_controller.cpp` — player controller (fixed 60 Hz, accel/decel,
  jump/gravity, camera-relative input, destination support, wall/overhead/edge handling)
- `source/gameplay/player_spawn.cpp` — spawn override resolution and validation
- `source/core/stage.cpp` — STG/HIT parsers
- `source/core/math.cpp/.hpp` — mat4, TRS composition T*Rz*Ry*Rx*S
- `source/render/stage_renderer.cpp` — HIT wireframe debug + stage auto-framing
- `tests/test_stage_collision.cpp`, `tests/test_player_controller.cpp`,
  `tests/test_player_spawn.cpp`
## 12. ELF binary analysis � SLES_541.61

### File identification

Analyzed C:\Dev\SuperDragonBallZ\assets\original\SLES_541.61 (4,180,784 bytes).

| Field | Value |
|---|---|
| Class | ELF32, little-endian |
| Machine | MIPS (0x0008) � PS2 R5900 |
| Type | Executable (0x0002) |
| Entry point | 0x00100008 |
| ELF flags | 0x20924000 (PS2 MIPS-III / R5900 hint) |
| Program headers | 2 at offset 0x34 |
| Section headers | 8 at offset 0x3FC9F0 |
| .text section | addr=0x00100000, off=0x00000080, size=0x003FC900 |

The entire executable is essentially one .text section spanning 0x00100000�0x004FC980.
There is no separate .data section; the ELF is stripped (.symtab and .strtab have
zero size, so no function symbols or debug names are available).

### COP2 (vector unit) instruction scan

Scanned the full .text section for MIPS COP2 instructions (primary opcode 0x10), which
on the PS2 R5900 are the VU vector math instructions used for collision/physics math
(vaddy, vaddz, vrsqrt Q, vwaitq, vmulq.xyz, etc.).

| Metric | Value |
|---|---|
| Total COP2 instructions | 8,097 |
| COP2 fmt values used | All 32 (0x00�0x1F) |
| Most common fmts | 0x04 (784), 0x10 (490), 0x11 (365), 0x09 (269), 0x0A (369) |
| jal instructions | 54,920 |

### COP2 instruction clusters

97 clusters of 5+ COP2 instructions within 64 bytes were found. These clusters
represent dense vector math regions � likely collision/physics functions. Top clusters:

| Address | Size | COP2 count |
|---|---|---|
| 0x004B4F28 | 0xCC | 51 |
| 0x004AC1E8 | 0x88 | 34 |
| 0x004BEA0C | 0x7C | 31 |
| 0x004BE56C | 0x7C | 31 |
| 0x0049AAF0 | 0x50 | 20 |
| 0x004AB710 | 0x48 | 18 |
| 0x0048DF70 | 0x48 | 18 |
| 0x0048AE60 | 0x48 | 18 |
| 0x004FA718 | 0x48 | 18 |
| 0x004FA5E0 | 0x40 | 16 |

The 0x0048xxxx�0x004Fxxxx range contains the densest concentration of COP2 clusters
(21 of the top 25 clusters), suggesting this region contains the core vector math /
collision code. The largest cluster at 0x004B4F28 has 51 consecutive COP2 instructions
(0xCC bytes), consistent with a heavy vector math routine.

### Notable COP2 instruction words

| Word | Count | fmt | func | Notes |
|---|---|---|---|---|
| 42000038 | 17 | 0x10 | 0x38 | Frequent vector computational op |
| 42000039 | 3 | 0x10 | 0x39 | Related vector op |
| 42000018 | 5 | 0x10 | 0x18 | Vector computational op |

These appear at addresses including 0x00175218, 0x00175230, 0x00175D6C, 0x00177F94,
0x00179910 (for 42000038) and 0x0017E8FC, 0x0017EA30, 0x0017EB38 (for 42000018).

### Candidate function addresses by collision subsystem

Because the ELF is stripped and no disassembler was available, no function addresses
can be definitively mapped to specific routines. However, COP2 cluster density and
instruction patterns provide candidates for each collision subsystem:

| Collision subsystem | Candidate address(es) | Evidence |
|---|---|---|
| Stage ground query (find walkable surface at XZ) | 0x004B4F28, 0x0048DF70 | Largest COP2 cluster (51 instrs); dense vector math consistent with plane-height queries and point-in-polygon tests |
| Capsule support checks | 0x004AC1E8, 0x004BEA0C | 34 and 31 COP2 instrs; consistent with ring sampling and footprint validation |
| HIT polygon normal computation (Newell's method) | 0x004BE56C, 0x0049AAF0 | 31 and 20 COP2 instrs; Newell's method requires cross-product vector ops |
| Spatial grid construction | 0x0048AE60, 0x004FA718 | 18 COP2 instrs each; cell index computation uses integer math mixed with vector ops |
| Wall blocking / sliding resolution | 0x004FA5E0, 0x004FA7D8 | 16 and 14 COP2 instrs; plane distance and tangent projection |
| Overhead clearance | 0x004F9FE8, 0x004FA6B8 | 14 and 12 COP2 instrs; vertical column vs capsule volume |
| Edge fall detection | 0x00491644, 0x004A9D98 | 12 and 11 COP2 instrs; ledge rejection and footprint boundary checks |
| Spawn / floor-finder routines | 0x004AC1E8 region | 34 COP2 instrs; spawn ranking combines height delta, clearance, and area |

These are **candidates only** � without disassembly or symbol information, the exact
purpose of each cluster cannot be confirmed.

### COP2 instruction patterns relevant to specific collision math

| Instruction word | Count | Likely vector op | Relevance to collision |
|---|---|---|---|
| 42000038 | 17 | fmt=0x10, func=0x38 | Frequent computational op; appears at 0x00175218, 0x00175230, 0x00175D6C, 0x00177F94, 0x00179910 |
| 42000039 | 3 | fmt=0x10, func=0x39 | Related vector op |
| 42000018 | 5 | fmt=0x10, func=0x18 | Vector computational op; appears at 0x0017E8FC, 0x0017EA30, 0x0017EB38 |

The `fmt=0x10` computational COP2 ops (490 total) are the most likely candidates for
Newell's method normal computation, plane height queries, and capsule ring sampling.

### Entry point code

First instructions at entry 0x00100008:

```
0x00100008: 70000C28
0x0010000C: 70001428
0x00100010: 70001C28
0x00100014: 70002428
```

Pattern `7000XX28` repeats with XX incrementing by 8 � suggests register initialization
or a table-like structure at the entry point.

### Collision flag pattern scan (0x1=wall, 0x2=floor, 0x4=event)

Scanned for andi/ori/addiu/beq/bne instructions using immediate values 0x1, 0x2, 0x4,
0x7, 0xF, 0x10:

Total flag-related instruction references: 51,068. Most are false positives (entry point
code, system call setup, general control flow). No collision flag references were found
within 256 bytes of any COP2 cluster, suggesting flag checking occurs in separate code
from the vector math. Without a disassembler, it is not possible to filter these down to
actual collision flag checks.

### SLUS_214.42 comparison

**SLUS_214.42 binary is not available** in this environment. Only SLES_541.61 (PAL/European)
is present. The known address shift between regions is **-0x2510**, meaning:

- SLES address = SLUS address - 0x2510
- SLUS address = SLES address + 0x2510

This shift cannot be verified without the SLUS binary.

### Identifiable function addresses

**No specific function addresses can be definitively identified** for the following
routines because the ELF is stripped (no symbols) and no disassembler was available:

- Stage ground query
- Capsule support test
- Horizontal movement / wall resolution
- Overhead clearance
- Edge fall detection
- Spawn / floor-finder routines

The COP2 clusters listed above (particularly 0x004B4F28, 0x004AC1E8, 0x0048DF70,
0x0048AE60, 0x0049AAF0) are candidate locations for vector-heavy collision functions,
but their exact purpose cannot be determined without disassembly or symbol information.

### Limitations

- Stripped ELF: no .symtab, no function names
- No SLUS_214.42 binary for cross-region comparison
- No MIPS/R5900 disassembler available for instruction-level analysis
- PowerShell byte-by-byte scanning is slow; full string/symbol extraction not feasible
- The C code in this repository is a reimplementation, not a disassembly of the original
  binary, so ELF addresses do not directly map to source line numbers


## 13. ELF binary analysis � hitbox/damage collision (SLES_541.61)

### Fighter attack hitbox activation

No specific function addresses could be identified for fighter attack hitbox
activation. The ELF is stripped (no .symtab, no function names), and no MIPS/
R5900 disassembler was available for instruction-level analysis. Without symbols
or a disassembler, it is not possible to map specific ELF addresses to
hitbox activation routines.

### Hitbox?HIT polygon intersection

No specific function addresses could be identified. The COP2 instruction scan
(section 12) found 8,097 COP2 (vector unit) instructions across the .text
section, with 97 dense clusters of 5+ COP2 instructions within 64 bytes. The
largest clusters are candidate locations for vector-heavy collision math
(including hitbox?HIT polygon intersection):

| Cluster address | COP2 count | Size |
|---|---|---|
| 0x004B4F28 | 51 | 0xCC |
| 0x004AC1E8 | 34 | 0x88 |
| 0x004BEA0C | 31 | 0x7C |
| 0x004BE56C | 31 | 0x7C |
| 0x0049AAF0 | 20 | 0x50 |
| 0x0048DF70 | 18 | 0x48 |
| 0x0048AE60 | 18 | 0x48 |
| 0x004FA718 | 18 | 0x48 |
| 0x004FA5E0 | 16 | 0x40 |

No collision flag references (andi/ori with 0x1, 0x2, 0x4) were found within
256 bytes of any COP2 cluster, suggesting flag checking occurs in separate code
from the vector math.

### Damage calculation on collision

11,197 arithmetic instructions (mul, div, mflo, mfhi) were found across the
.text section. These are candidate locations for damage calculation code, but
without a disassembler it is not possible to distinguish damage multiplication
from other arithmetic (e.g. coordinate transformation, physics integration).
No specific damage-related function addresses could be identified.

### Collision response (knockback, slide, bounce)

No specific function addresses could be identified. The 11,197 mul/div/mflo/
mfhi instructions and 8,097 COP2 instructions are candidate locations for
knockback/slide/bounce vector math, but cannot be confirmed without
disassembly.

### Hurtbox/hitbox interaction between fighters

No specific function addresses could be identified. No patterns unique to
fighter-vs-fighter collision were found in the byte-level scan.

### Collision flag pattern scan (0x1=wall, 0x2=floor, 0x4=event)

Scanned for andi/ori/addiu/beq/bne instructions using immediate values 0x1,
0x2, 0x4, 0x7, 0xF, 0x10:

| Instruction type | Count |
|---|---|
| andi with flag imm | (subset of 51,068 total) |
| ori with flag imm | (subset of 51,068 total) |
| addiu with flag imm | (subset of 51,068 total) |
| beq/bne with small imm | (subset of 51,068 total) |

Total flag-related instruction references: 51,068. Most are false positives
(entry point code, system call setup, general control flow). Without a
disassembler, it is not possible to filter these down to actual collision flag
checks.

### SLUS_214.42 comparison

**SLUS_214.42 binary is not available** in this environment. Only
SLES_541.61 (PAL/European) is present. The known address shift between regions
is **-0x2510**, meaning:

- SLES address = SLUS address - 0x2510
- SLUS address = SLES address + 0x2510

This shift cannot be verified without the SLUS binary. Therefore, no comparison
of hitbox/damage collision logic between SLES and SLUS can be performed.

### Identifiable function addresses

**No specific function addresses can be definitively identified** for:

- Fighter attack hitbox activation
- Hitbox?HIT polygon intersection
- Damage calculation on collision
- Collision response (knockback, slide, bounce)
- Hurtbox/hitbox interaction between fighters

### Limitations

- Stripped ELF: no .symtab, no function names
- No SLUS_214.42 binary for cross-region comparison
- No MIPS/R5900 disassembler available for instruction-level analysis
- PowerShell byte-by-byte scanning cannot distinguish collision-specific code
  from general arithmetic/control flow
- The C code in this repository is a reimplementation, not a disassembly of
  the original binary, so ELF addresses do not directly map to source line
  numbers

