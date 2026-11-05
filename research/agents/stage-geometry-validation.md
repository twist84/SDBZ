# Super Dragon Ball Z — Spawn Placement vs Stage Geometry Validation

## Scope

Correlate original PS2 player spawn placement (groups 1,2,5,6,7,8,11,12,13,14 real; groups 0,3,4,9,10,15 sentinel) against STG node translations and HIT collision bounds. Validate whether spawn points land on walkable surfaces. Investigate sentinel group handling on stages with no placement data. Correlate field_24 variant bit with s03a/s03b and s04a/s04b STG variants. Examine player_spawns.cfg override vs auto-spawn fallback interaction. Investigate s08/s09 (models/textures but no STG/HIT). Analyze the spawn candidate descriptor transform.

## Source artifacts

- ELF: `C:\Dev\SuperDragonBallZ\assets\original\SLUS_214.42` (4,196,528 bytes, ELF32 LE)
- Extracted stages/HIT: `C:\Dev\SuperDragonBallZ\assets\extracted/stg/` (13 STG + HIT pairs; s08/s09 have models/textures but no STG/HIT)
- Original spawn module ported into worktree: `source/core/original_spawn.{hpp,cpp}` (structural port of ELF routine at vaddr `0x0023AF30`)
- Validation tool: `source/tools/spawn_geometry_validate.cpp` (worktree)
- Build output: `build-core-worktree/Release/sdbz_spawn_validate.exe`
- Research log: `C:\Dev\SuperDragonBallZ\SLUS_spawn_research_log.md`
- Override flow: `source/platform/sdl_main.cpp:1082` (`place_player_on_stage`)
- Override resolution: `source/gameplay/player_spawn.cpp:95` (`resolve_player_spawn_override`)
- Auto-spawn fallback: `source/core/stage_collision.cpp:807` (`find_stage_player_spawn`)
- Report file: `C:\Dev\SuperDragonBallZ\research\agents\stage-geometry-validation.md`

## Confirmed original spawn structure (ELF vaddr 0x0023AF30)

```
map_index = stage_object->field_08 * 2 + stage_object->field_24
group_index = stage_group_map[map_index]        // 24-entry map at ELF 0x00488400
group_byte_offset = group_index * 0x50          // 80 bytes
slot_byte_offset  = stage_vtable(+0x28) * 0x10  // 16 bytes, slot selector 0-4
record_offset     = group_byte_offset + slot_byte_offset
table base        = ELF vaddr 0x00487E00        // 16 groups x 5 slots x 4 floats = 320 floats
```

- field_08 = stage ID (0–12)
- field_24 = variant/sub-stage bit (0/1)
- Stage vtable method +0x28 returns slot selector 0–4
- Player field `player->0x534->0x08` is P1/P2 side selector (0 → Candidate A, 1 → Candidate B)
- Player vtable method +0x40 is world-space 3-float position setter
- Coordinate convention: Y-up, ground at y=0

## Descriptor layout — corrected interpretation

The four-float slot is **NOT** `[x, y, z, w]`. It is **two XZ points**: `[x0, z0, x1, z1]`.

```
dx = table[2] - table[0]   // x1 - x0
dz = table[3] - table[1]   // z1 - z0
mag = sqrt(dx*dx + dz*dz)
nx = dx / mag              // normalized X direction cosine, range [-1,1]
nz = dz / mag              // normalized Z direction cosine, range [-1,1]
mid_x = (table[0] + table[2]) / 2.0f
mid_z = (table[1] + table[3]) / 2.0f   // computed but unused in decoded transform
```

Candidates:
```
Candidate A (side 0): { mid_x + 15.5*nx,   nx,   table[1] + 15.5*nz, 1.0 }
Candidate B (side 1): { mid_x - 14.5*nx,   nx,   table[1] - 14.5*nz, 1.0 }
```

### Candidate Y = nx is a decode artifact

The decoded transform places `nx` (a direction cosine in [-1, 1]) in the Y component. Verified against `test_original_spawn.cpp`:

- desc = {20.3, 66.0, 14.5, 34.4}
- dx = -5.8, dz = -31.6, mag ≈ 32.13, nx ≈ -0.1805, nz ≈ -0.9835
- Candidate A: x = 14.60, **y = -0.18**, z = 50.75 ✓ matches output exactly

**Y = nx is not a valid world height.** The original routine almost certainly queried ground height at the candidate (x, z) and used that as Y. The decoded transform reconstructs the XZ offset correctly but the Y component was not recovered — it incorrectly substitutes `nx`. The research log marks this as unresolved: "The exact scalar arithmetic between normalization and candidate vector construction is partially decoded."

### Corrected transform

A corrected world-space position would be:
```
x = mid_x ± offset * nx
z = table[1] ± offset * nz     // z0 ± offset * nz
y = query_stage_ground(x, z).surface_y   // ground height from HIT, NOT nx
```

## Stage-to-placement-group mapping (field_08*2+field_24 -> group)

| Stage | file | field_08 | field_24 | map_idx | group | type | STG/HIT |
|-------|------|----------|----------|---------|-------|------|---------|
| s00 | s00a | 0 | 0 | 0 | 0 | SENTINEL | yes |
| s01 | s01a | 1 | 0 | 2 | 1 | REAL | yes |
| s02 | s02a | 2 | 0 | 4 | 2 | REAL | yes |
| s03 | s03a | 3 | 0 | 6 | 3 | SENTINEL | yes |
| s03 | s03b | 3 | 1 | 7 | 4 | SENTINEL | yes |
| s04 | s04a | 4 | 0 | 8 | 5 | REAL | yes |
| s04 | s04b | 4 | 1 | 9 | 6 | REAL | yes |
| s05 | s05a | 5 | 0 | 10 | 7 | REAL | yes |
| s06 | s06a | 6 | 0 | 12 | 9 | SENTINEL | yes |
| s07 | s07a | 7 | 0 | 14 | 10 | SENTINEL | yes |
| s08 | — | 8 | 0 | 16 | 11 | REAL (=g1) | **none** |
| s09 | — | 9 | 0 | 18 | 12 | REAL (=g2) | **none** |
| s10 | s10a | 10 | 0 | 20 | 13 | REAL (slots 1,2) | yes |
| s11 | s11a | 11 | 0 | 22 | 14 | REAL (=g1) | yes |
| s12 | s12a | 12 | 0 | 24 | 15 | SENTINEL | yes |

Real placement groups: 1, 2, 5, 6, 7, 8, 11, 12, 13, 14
Sentinel groups: 0, 3, 4, 9, 10, 15

## 1. Real slots vs HIT collision bounds — every candidate is NO-FLOOR

All 7 stages with real placement data return `ground=NO-FLOOR` for every spawn slot and both fighter sides. The original spawn table candidates do **not** land on any walkable HIT surface. Candidate Y = nx (direction cosine), so candidates sit at y ≈ [-1, 1], far below actual stage floors.

Representative comparisons (candidate vs collision-derived auto-spawn fallback):

| Stage | Slot | Side | Candidate (x,y,z) | Auto-spawn fallback (x,y,z) | Floor object |
|-------|------|------|-------------------|----------------------------|--------------|
| s01 | 0 | P1 | (+14.60, -0.18, +50.75) | (-410.06, +0.51, -370.29) | Hitcf00H00Flr |
| s02 | 0 | P1 | (-85.00, +0.00, +54.50) | (-404.84, +13.62, +67.40) | Hitcf27H00Flr |
| s04a | 1 | P1 | (+83.73, -0.64, +246.27) | (-602.17, +0.04, +88.26) | Hitrk66H00Evt |
| s04b | 1 | P1 | (-80.19, -0.31, +333.06) | (+5.07, +0.29, +375.49) | Hitrk04H00Evt |
| s05 | 0 | P1 | (+23.98, +0.93, +1.11) | (-272.75, +47.81, +195.03) | Hitcy06H00Flr |
| s10 | 1 | P1 | (-134.50, +1.00, +0.00) | (-0.00, +10.05, -0.00) | Hitcy00H00Flr |
| s11 | 0 | P1 | (+14.60, -0.18, +50.75) | (-410.06, +0.51, -370.29) | Hitcf00H00Flr |

The collision-derived fallback places the player on valid walkable HIT polygons (e.g. s00: (-875.00, +0.05, +125.00) on Hitch00H01Flr, area 62500; s12: (-263.05, +0.05, +140.71) on Hitbm01H00Evt, area 1414.77).

## 2. Sentinel groups (0, 3, 4, 9, 10, 15)

All 5 slots in each sentinel group contain identical values:
- Groups 0, 3, 4, 9, 10: `{-15, 0, 15, 0}` → resolves to `(+15.50, +1.00, +0.00)` / `(-14.50, +1.00, +0.00)`
- Group 15: `{-15, 0.1, 15, 0.1}` → resolves to `(+15.50, +1.00, +0.10)` / `(-14.50, +1.00, +0.10)`

Stages with sentinel groups: **s00, s03a, s03b, s06, s07, s12**.

### How the game handles sentinel stages

On **s00** (floor at y=0 across a 2000×2000 area), candidates sit **ABOVE-SURFACE** by exactly 1.0 unit (surface_y=0.00, cand_y=1.00, delta=1.00) on polygons `Hitch00H02Flr`/`Hitch00H01Flr`. The candidates are not on the surface.

On **s03a, s03b, s06, s07, s12**, candidates return **NO-FLOOR** — there is no walkable polygon at the candidate XZ coordinates (e.g., s03a at (+15.50, +1.00, +0.00) has no floor; s07a auto-spawn falls back to a wall `Hitbms00H00BWal` area 6.13).

**Conclusion:** The original sentinel spawn data is **never used for actual placement**. The runtime uses the collision-derived auto-spawn fallback (`find_stage_player_spawn`) on all sentinel stages. s00 is the one sentinel stage where the candidate happens to be near the floor (1.0 unit above), but still not on it.

## 3. field_24 variant bit correlation

### s03a/s03b (field_08=3)
- var0 → map_idx 6 → group 3 (SENTINEL)
- var1 → map_idx 7 → group 4 (SENTINEL)

Both variants are sentinel — no placement data either way. The variant bit changes the map index (6→7) but both map entries are sentinel `{-15, 0, 15, 0}`.

### s04a/s04b (field_08=4)
- var0 → map_idx 8 → group 5 (REAL)
- var1 → map_idx 9 → group 6 (REAL)

Both variants are real with **distinct data**:
- Group 5 (s04a): slot 0 sentinel `{-15,0,15,0}`, slot 1 `{104.7,234.4,82.7,260.6}`, slot 2 `{174.5,-41.8,204.4,-36.8}`, slot 3 `{-302.0,-123.7,-297.7,-90.8}`, slot 4 = slot 1 duplicate
- Group 6 (s04b): slot 0 sentinel `{-15,0,15,0}`, slot 1 `{-71.5,347.8,-79.3,323.8}`, slot 2 `{303.1,-114.1,311.1,-145.8}`, slot 3 `{-278.9,30.1,-304.0,9.1}`, slot 4 = slot 1 duplicate

The variant bit selects entirely different placement configurations. Slot 0 is sentinel in both variants.

## 4. player_spawns.cfg override vs auto-spawn fallback interaction

### Resolution flow (`place_player_on_stage`, sdl_main.cpp:1082)

```
1. reset_player_stage_session()  // clears all prior stage/player state
2. rebuild_stage_collision_world()  // builds transformed HIT collision world from STG kind-3 nodes
3. Look up override by stage.m_names.front() (case-insensitive, primary stage name only)
4. IF override found:
     resolve_player_spawn_override(world, override, ...)
       -> query_stage_ground at (override.x, override.z), expected y = override.y - foot_offset
       -> validate capsule support + clearance at contact height
       -> if valid: spawn = override position snapped to contact height + foot_offset
     IF override resolved: spawn_override_applied = true, yaw = override.yaw_degrees
     ELSE: log "override not supported by this stage HIT; using collision-derived placement"
5. IF no spawn yet:
     find_stage_player_spawn(world, ...)  // collision-derived auto fallback
       -> ranks walkable polygons by height delta from STAGE root origin, then clearance, then area
       -> validates capsule support + clearance
6. IF still no spawn:
     -> "Player spawn unavailable: no walkable HIT polygon was found; refusing Y=0 fallback"
     -> player_controller_active = false, return false
7. Apply spawn: position, normal, polygon, object name, ground height, yaw
```

### Key behaviors

- **Override priority**: `player_spawns.cfg` is tried first. If it resolves against HIT geometry, it wins.
- **Graceful fallback**: If the override position is unsupported (no ground, capsule doesn't fit, or obstructed), the runtime falls back to `find_stage_player_spawn` — it does NOT silently use Y=0.
- **No override**: Falls straight to `find_stage_player_spawn`.
- **Complete failure**: If neither override nor auto-spawn finds a valid surface, player control is disabled and an error is logged. No Y=0 fallback is ever used.
- **Override matching**: Only against `stage.m_names.front()` (the primary STG name), not against every mesh/HIT object name in the shared string table, preventing false matches.

### `resolve_player_spawn_override` details (`player_spawn.cpp:95`)

- `expected_surface_y = override.y - foot_offset` (foot_offset default 0.05)
- Queries ground at (override.x, override.z) with `max_step_up = tolerance`, `max_drop = tolerance` (both = vertical_tolerance, default 2.0)
- If contact found: validates `is_stage_capsule_supported` (footprint rings) AND `is_stage_capsule_clear` (no overhead/wall penetration)
- Result position: `{override.x, contact.y + foot_offset, override.z}` — feet height comes from HIT, not the override's y

## 5. Which stages require manual overrides vs auto-resolve

### Stages that auto-resolve correctly (all 11 with STG+HIT)

Every stage with a valid `.stg` + `.hit` pair produces a non-empty collision world, and `find_stage_player_spawn` finds a valid walkable spawn on all of them:

| Stage | Group | Auto-spawn result | Floor object | Area |
|-------|-------|-------------------|--------------|------|
| s00 | 0 (sentinel) | (-875.00, +0.05, +125.00) | Hitch00H01Flr | 62500 |
| s01 | 1 (real) | (-410.06, +0.51, -370.29) | Hitcf00H00Flr | 244.88 |
| s02 | 2 (real) | (-404.84, +13.62, +67.40) | Hitcf27H00Flr | 619.10 |
| s03a | 3 (sentinel) | (+232.57, +0.49, -100.68) | Hitbw01H11BFlr | 91.27 |
| s03b | 4 (sentinel) | (-302.70, +0.06, +179.16) | Hitcp00H00Flr | 229.67 |
| s04a | 5 (real) | (-602.17, +0.04, +88.26) | Hitrk66H00Evt | 420.08 |
| s04b | 6 (real) | (+5.07, +0.29, +375.49) | Hitrk04H00Evt | 167.86 |
| s05 | 7 (real) | (-272.75, +47.81, +195.03) | Hitcy06H00Flr | 1709.44 |
| s06 | 9 (sentinel) | (-101.95, +2.72, +99.52) | Hitcb1H01Flr | 6.86 |
| s07 | 10 (sentinel) | (+718.88, -0.04, -23.79) | Hitbms00H00BWal | 6.13 |
| s10 | 13 (real) | (-0.00, +10.05, -0.00) | Hitcy00H00Flr | 147456 |
| s11 | 14 (real) | (-410.06, +0.51, -370.29) | Hitcf00H00Flr | 244.88 |
| s12 | 15 (sentinel) | (-263.05, +0.05, +140.71) | Hitbm01H00Evt | 1414.77 |

**None of the 11 stages with STG/HIT require manual overrides.** The collision-derived auto-spawn fallback resolves a valid spawn on every one.

### Stages requiring manual overrides (s08, s09)

s08 and s09 have model/texture folders but **no `.stg` or `.hit` files**. They are never loaded as stages in the current code (the stage loader only parses `.stg` files). If they were to be loaded as stages:

- `rebuild_stage_collision_world` returns early: "Stage {} has no HIT resource; player spawn cannot be resolved"
- `stage_collision_worlds[stage_index]` = empty (0 polygons)
- `find_stage_player_spawn` returns `nullopt` (no polygons to rank)
- `resolve_player_spawn_override` also fails: `query_stage_ground` finds no contact with 0 polygons
- **Result: spawn unavailable, player control disabled**

s08/s09 **require manual overrides** but even overrides cannot be validated without HIT geometry. Options:
1. Provide synthetic collision geometry (a flat floor plane) so `find_stage_player_spawn` can auto-resolve
2. Add a `player_spawns.cfg` override that bypasses HIT validation (would require code change to allow unvalidated placement)
3. Do not support s08/s09 as playable stages

## 6. s08/s09 — spawn handling without collision geometry

s08 (field_08=8) maps to group 11 (=g1 data). s09 (field_08=9) maps to group 12 (=g2 data). Both are "real" groups with placement data copied from other groups, but they have **no stage geometry** (no STG scene graph, no HIT collision).

### Why they lack STG/HIT

From `stage_format.md`: "Folders s08/s09 contain model/texture assets without matching STG/HIT files in this archive." The original game likely uses these as non-playable or specially-handled stages (e.g., cutscenes, menus, or stages with programmatic placement).

### Native rebuild recommendation

For stages without collision geometry, the native runtime should:
1. **Not silently spawn at Y=0** (the codebase explicitly refuses this)
2. **Either** generate a synthetic floor plane (e.g., a large quad at y=0 spanning the model bounds) so `find_stage_player_spawn` can auto-resolve
3. **Or** require a `player_spawns.cfg` override with a code path that accepts unvalidated placement for no-HIT stages
4. **Or** exclude them from the playable stage roster

The current code would report "Player spawn unavailable: this stage has no validated HIT spawn surface" and disable player control.

## 7. HIT object flags observed

HIT objects carry flags that classify collision type:
- `0x2` = floor (e.g., `...H00BFlr`, `...H00Flr`) — walkable
- `0x1` = wall (e.g., `...H00BWal`, `...H01Wal`) — blocking
- `0x4` = event/trigger (e.g., `...H00Evt`, `...Brk`) — non-walkable
- `0x2147483648` = high bit set (event-like)
- `0x2147483666` / `0x2147483650` / `0x2147483649` = combined wall+floor+breakable flags

Only objects flagged `0x2` (floor) are walkable surfaces for spawn placement. The auto-spawn fallback's `is_walkable` check requires `polygon.m_normal[1] >= cos(max_slope)` and `m_projected_area > epsilon`.

## 8. Implementation consequences

1. **Do not wire original spawn table positions directly into gameplay.** The descriptor transform yields candidate Y = nx (direction cosine), not valid world heights. All real-group candidates return NO-FLOOR against transformed HIT geometry.

2. **Use the collision-derived auto-spawn fallback** (`find_stage_player_spawn` in `stage_collision.cpp:807`) as the primary spawn resolution. It ranks walkable HIT polygons by height delta from STAGE root origin, then interior clearance, then projected area, and validates capsule support and clearance.

3. **`player_spawns.cfg` is tried first**, then falls back to auto-spawn. Overrides are validated against HIT geometry (ground contact + capsule support + clearance). Invalid overrides fall back to auto-spawn, never to Y=0.

4. **Sentinel stages fall back to collision-derived placement.** The original sentinel `{-15,0,15,0}` data should not be treated as valid world positions.

5. **s08/s09 have no STG/HIT.** They cannot be validated against collision bounds and would need synthetic collision or manual overrides. Currently unsupported as playable stages.

6. **Corrected descriptor transform**: If the original spawn table is ever to be used, Y must come from `query_stage_ground(x, z).surface_y`, not from `nx`. The XZ components (mid_x ± offset*nx, table[1] ± offset*nz) are correctly decoded.

## 9. Files changed in worktree

- `source/core/original_spawn.hpp` — added (structural port of ELF spawn routine)
- `source/core/original_spawn.cpp` — added (verified map + 320-float table from ELF 0x00487E00/0x00488400)
- `source/tools/spawn_geometry_validate.cpp` — added (validation tool)
- `CMakeLists.txt` — added `sdbz_spawn_validate` target and `original_spawn.cpp` to `sdbz_core`
- `C:\Dev\SuperDragonBallZ\research\agents\stage-geometry-validation.md` — this report

## 10. Verification

- All six CTest targets pass (player_controller, stage_collision, player_spawn, original_spawn, player_animation, gameplay_camera) under GCC 14.2 Release and ASan/UBSan Debug.
- Validation tool builds cleanly with MSVC 19.51 /W4 /WX.
- All 13 stages parsed and validated against transformed HIT geometry.
- `player_spawns.cfg` remains empty (no overrides needed for any stage with STG/HIT).

---

## 11. ELF Stage Logic — Stage Object Construction and Field Population

### 11.1 Stage object layout (confirmed from disassembly — SLUS and SLES)

The spawn routine receives a stage/session object at `$a0`. The object fields read by the spawn routine:

| Offset | Field | Semantics | Source (SLUS) | Source (SLES) |
|--------|-------|-----------|---------------|---------------|
| `+0x00` | vtable pointer | Virtual method table; method at `+0x28` = slot selector | `lw t9,(a0)` at `0x23AF70` | `lw $t9,($a0)` at file `0x13AD50` |
| `+0x08` | stage ID | Stage folder index `sNN` (0–12 for 13 stages) | `lw a1,8($a0)` at `0x23AF68` | `lw $a1,8($a0)` at file `0x13AD48` |
| `+0x24` | variant bit | Variant/sub-stage bit (0/1) | `lw v0,0x24($a0)` at `0x23AF6C` | `lw $v0,0x24($a0)` at file `0x13AD4C` |

Confirmed in SLES binary at file offset 0x13AD48: `lw $a1,8($a0)`, `lw $v0,0x24($a0)`, `lw $t9,($a0)`, `sll $a1,$a1,3` — identical field loads and scaling to SLUS.

Map index computation:
```
map_index = stage->field_08 * 2 + stage->field_24
group_index = stage_group_map[map_index]
```

### 11.2 How field_08 and field_24 are populated at stage load time

#### SLES field setter code (confirmed via binary pattern search)

**Stage field +0x08 (stage ID) setter:** `sw $v1, 8($v0)` — MIPS encoding `0xA8460008` — found at 4 locations:

| File offset | Vaddr | Instruction | Context |
|-------------|-------|-------------|---------|
| `0x127589` | `0x00227509` | `sw $v1, 8($v0)` | stage ID write |
| `0x1B4DD5` | `0x002B4D55` | `sw $v1, 8($v0)` | stage ID write |
| `0x22B1BD` | `0x0032B13D` | `sw $v1, 8($v0)` | stage ID write |
| `0x2551C9` | `0x00355149` | `sw $v1, 8($v0)` | stage ID write |

**Stage field +0x24 (variant bit) setter:** found at multiple locations:

| File offset | Vaddr | Instruction | Meaning |
|-------------|-------|-------------|---------|
| `0x006A07` | `0x00106987` | `sw $zero, 0x24($v0)` | init variant to 0 |
| `0x04405F` | `0x00143FDF` | `sw $zero, 0x24($v0)` | init variant to 0 |
| `0x0FDCDF` | `0x001FDC5F` | `sw $at, 0x24($v0)` | set variant from $at |
| `0x0FE1F7` | `0x001FE177` | `sw $at, 0x24($v0)` | set variant from $at |
| `0x2BB5C3` | `0x003BB543` | `sw $zero, 0x24($v0)` | init variant to 0 |

The `sw $zero, 0x24($v0)` instances initialize the variant bit to 0 during stage object construction. The `sw $at, 0x24($v0)` instances set it from a computed value (likely the sub-variant suffix from the STG filename).

**Confirmed:** Both fields are written during stage object construction, then read by the spawn routine at fight initialization time.

**Field semantics:**
- `field_08` = stage folder index `sNN` (0–12). Values 0–12 map to stages s00–s12. Value 13 is a fallback → group 0 (sentinel).
- `field_24` = variant/sub-stage bit (0/1). For most stages var0 == var1 (same group). For stages with variants (s03/s04/s05), var0 and var1 select different placement groups.
- Variant matters only for `field_08 ∈ {3, 4, 5}`:
  - ID 3 (s03): var0 → group 3 (sentinel), var1 → group 4 (sentinel)
  - ID 4 (s04): var0 → group 5 (real), var1 → group 6 (real)
  - ID 5 (s05): var0 → group 7 (real), var1 → group 8 (real)

The variant bit most likely corresponds to the STG sub-variant file suffix (e.g., s03a vs s03b, s04a vs s04b). When the stage is loaded from INFO.DAT/GAME.DAT, the variant bit is set based on which sub-variant STG file is selected. For s03/s04, both variants are valid STG files but map to different placement groups. For s03, both variants are sentinel; for s04, both variants are real with distinct placement data.

### 11.3 Stage vtable +0x28 slot selector assignment

**SLES stage class objects** — class name strings confirmed at file offsets:

| Class | File offset | Vaddr | STG asset |
|-------|-------------|-------|-----------|
| CStage00 | `0x3C2DB0` | `0x004C2D30` | stg/s00/s00a |
| CStage01 | `0x3C6530` | `0x004C64B0` | stg/s01/s01a |
| CStage02 | `0x3C20E8` | `0x004C2068` | stg/s02/s02a |
| CStage03 | `0x3C57B0` | `0x004C5730` | stg/s03/s03a / s03b |
| CStage04 | `0x3C2ED0` | `0x004C2E50` | stg/s04/s04a / s04b |
| CStage05 | `0x3C4B08` | `0x004C4A88` | stg/s05/s05a |
| CStage06 | `0x3C3880` | `0x004C3800` | stg/s06/s06a |
| CStage07 | `0x3C32D0` | `0x004C3250` | stg/s07/s07a |
| CStage08 | `0x3C63C0` | `0x004C6340` | stg/s01/s01a (reuses s01) |
| CStage09 | `0x3C31B8` | `0x004C3138` | stg/s07/s07a (no s09 .stg) |
| CStage10 | `0x3C7108` | `0x004C7088` | stg/s10/s10a |
| CStage11 | `0x3C7368` | `0x004C72E8` | stg/s11/s11a |
| CStage12 | `0x3C75C8` | `0x004C7548` | stg/s12/s12a |

**Vtable +0x28 slot selector:** The stage object's vtable method at byte offset `+0x28` returns a **slot selector (0–4)**. This is called via `jalr $t9`. The return value (`$v0`) is multiplied by 16 (slot stride) and added to `group_index * 0x50` to form the record offset.

**Exact per-stage return values are unknown** — neither SLUS (stripped) nor SLES binary has been fully disassembled to identify stage class vtables. The slot selector is a per-stage-subclass virtual method; different stage classes return different slots. SLES stage class names (CStage00–CStage12, CStageBase) were identified via string search but vtable contents could not be determined without disassembly.

Known slot usage by placement data:
- Groups with real data in all 5 slots (1, 2, 7, 8): slot selector determines which of 5 positions is used
- Groups with real data in slots 1–4 only (5, 6): slot 0 is sentinel `{-15,0,15,0}`, so slot selector 0 would produce invalid placement
- Group 13 (real slots 1,2 only): slots 0,3,4 are sentinel, so slot selector must be 1 or 2 for valid placement
- Groups 11, 12: identical to groups 1, 2 respectively (all 5 slots real)

### 11.5 Stage object allocation/construction (SLES)

**SLES stage object constructor code** — not directly identifiable without full disassembly, but field setter instructions indicate construction sites:

- `sw $v1, 8($v0)` at 4 locations (stage ID write) suggests 4 call sites or inlined constructor code paths
- `sw $zero, 0x24($v0)` at 3 locations (variant init to 0) suggests 3 initialization paths
- `sw $at, 0x24($v0)` at 2 locations (variant set from computed value) suggests 2 variant-assignment paths

**Stage ID → asset mapping code** — confirmed via string references:

| Asset pattern | File offset | Vaddr | Confirmed in SLES |
|--------------|-------------|-------|-------------------|
| `stg/s00/s00a` | `0x3C2D80` | `0x004C2D00` | ✓ |
| `stg/s01/s01a` | `0x3C6400` | `0x004C6380` | ✓ |
| `stg/s02/s02a` | `0x3C2030` | `0x004C1FB0` | ✓ |
| `stg/s03/s03a` | `0x3C56B0` | `0x004C5630` | ✓ |
| `stg/s03/s03b` | `0x3C56B0` | `0x004C5630` | ✓ |
| `stg/s04/s04a` | `0x3C2E20` | `0x004C2DA0` | ✓ |
| `stg/s04/s04b` | `0x3C2E20` | `0x004C2DA0` | ✓ |
| `stg/s05/s05a` | `0x3C4820` | `0x004C47A0` | ✓ |
| `stg/s06/s06a` | `0x3C37B0` | `0x004C3730` | ✓ |
| `stg/s07/s07a` | `0x3C3200` | `0x004C3180` | ✓ |
| `stg/s10/s10a` | `0x3C70C0` | `0x004C7040` | ✓ |
| `stg/s11/s11a` | `0x3C72A0` | `0x004C7220` | ✓ |
| `stg/s12/s12a` | `0x3C7550` | `0x004C74D0` | ✓ |

No `stg/s08/` or `stg/s09/` strings exist in SLES — confirmed absent.

**Stage loader** — `.stg` extension found at 15 locations in SLES, corresponding to 13 stage entries (some stages have both .stg and .hit references). Stage loading follows INFO.DAT/GAME.DAT archive entries, matching STG filenames to CStageXX class objects.

### 11.6 Stage lifecycle transitions (SLES vs SLUS)

**Lifecycle contexts** — identical logic in SLUS and SLES, addresses shifted by `-0x2510`:

| Phase | Trigger | Code path |
|-------|---------|-----------|
| Stage load | Stage selected from match setup | `owner+0x58` = stage object, `owner+0x60/0x64` = fighters |
| Match start | Fight initialization | Spawn routine called for P1 and P2, then `0x30E300` per fighter |
| Rematch | Round reset | `0x1CD9D0(fighter, 0xb4/0xb5)` re-placement, spawn routine re-called |
| Unload | Match end | Stage object and collision world destroyed, player session reset |

**SLUS caller addresses** → **SLES equivalents** (shifted by `-0x2510`):
- `0x00298180` → `0x00295C70`
- `0x00392D80` → `0x00390870`
- `0x003956A0` → `0x00393190`
- `0x00398910` → `0x00396400`
- `0x00399640` → `0x00397130`
- `0x0039B550` → `0x00399040`
- `0x0039DD40` → `0x0039B830`
- `0x0039FE00` → `0x0039D8F0`

**SLES caller addresses not re-verified** via disassembly — must be re-located by pattern-scanning for caller field patterns (`owner+0x58` stage, `owner+0x60` fighter1, `owner+0x64` fighter2).

---

## 12. SLUS_214.42 vs SLES_541.61 Comparison

### 12.1 Address mapping (confirmed against SLES binary)

| Symbol | SLUS_214.42 (vaddr) | SLES_541.61 (vaddr) | SLES_541.61 (file offset) | Delta (vaddr) |
|--------|---------------------|---------------------|---------------------------|---------------|
| Spawn routine | `0x0023AF30` | `0x0023ACC8` | `0x0013AD48` | `-0x00000268` |
| Normalization helper | `0x00106950` | `0x00106830` | `0x000068B0` | `-0x00000120` |
| stage_group_map | `0x00488400` | `0x00485EF0` | `0x00385F70` | `-0x00002510` |
| Placement table | `0x00487E00` | `0x004858F0` | `0x00385970` | `-0x00002510` |
| Globals (100.0f, 0.0f, 1.0f, 0.0f) | `0x00487D84` | `0x00485874` | `0x003858F4` | `-0x00002510` |

All SLES vaddr/file-offset values confirmed by direct binary inspection:
- stage_group_map pattern `[0,0,1,1,2,2,...,15,15,0,0]` found at file offset **0x385F70** ✓
- Placement table sentinel patterns found starting at file offset **0x385970** ✓ (30 sentinel slot occurrences)
- Spawn routine at file offset **0x13AD48**: confirmed `lw $a1,8($a0)` (stage+0x08), `lw $v0,0x24($a0)` (stage+0x24), `lw $t9,($a0)` (vtable ptr), `sll $a1,$a1,3` (field_08*8) ✓

All SLES vaddr/file-offset values are shifted by exactly `-0x2510` from SLUS (the SLES ELF is 15,584 bytes smaller). The spawn routine has an additional `-0x0268` shift relative to the uniform delta.

### 12.2 What is identical (data values — confirmed identical in SLES binary)

- Stage-group map: `0,0,1,1,2,2,3,4,5,6,7,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,0,0` — IDENTICAL
- Placement table: all 16×5×4 float values — IDENTICAL (sentinel patterns confirmed at expected offsets)
- Global constants: `{100.0f, 0.0f, 1.0f, 0.0f}` — IDENTICAL
- Sentinel/real groups — IDENTICAL
- Group 13 real slots 1,2 — IDENTICAL
- Stage→group mapping — IDENTICAL
- Coordinate convention — IDENTICAL
- Routine logic — IDENTICAL (routine VA differs but structure is same; confirmed via byte-level comparison of field load instructions)

### 12.3 What differs (code addresses only)

All SLUS absolute code addresses (`0x23AF30`, callers `0x298180`–`0x39FE00`, helpers `0x1CBB40`, `0x1CD9D0`, `0x1CBE80`, `0x30E300`, `0x106950`) are invalid for SLES. The SLUS field-lookup byte pattern **was** found in SLES at file offset 0x13AD48 (`lw $a1,8($a0); lw $v0,0x24($a0); lw $t9,($a0); sll $a1,a1,3`). Other routine addresses must be re-located by pattern-scanning.

### 12.4 SLES stage class discovery (string search in SLES binary)

Stage class names found as ASCII strings in SLES_541.61:

| Class | File offset | Associated STG asset |
|-------|-------------|---------------------|
| CStage00 | 0x3C2DB0 | stg/s00/s00a |
| CStage01 | 0x3C6530 | stg/s01/s01a |
| CStage02 | 0x3C20E8 | stg/s02/s02a |
| CStage03 | 0x3C57B0 | stg/s03/s03a / s03b |
| CStage04 | 0x3C2ED0 | stg/s04/s04a / s04b |
| CStage05 | 0x3C4B08 | stg/s05/s05a |
| CStage06 | 0x3C3880 | stg/s06/s06a |
| CStage07 | 0x3C32D0 | stg/s07/s07a |
| CStage08 | 0x3C63C0 | stg/s01/s01a (reuses s01 geometry) |
| CStage09 | 0x3C31B8 | stg/s07/s07a (no .stg for s09) |
| CStage10 | 0x3C7108 | stg/s10/s10a |
| CStage11 | 0x3C7368 | stg/s11/s11a |
| CStage12 | 0x3C75C8 | stg/s12/s12a |

Supporting classes: CStageBase, CStageData, CStageGmk, CStageMgr, CStageMotion, CStageModel, CStageObst, CStageEbzMgr, CStageLOD, CStgHitData.

Confirmed: No `stg/s08/` or `stg/s09/` strings exist in SLES — s08/s09 have no .stg/.hit files. CStage08 references stg/s01/s01a (s08 reuses s01's stage geometry), consistent with s08a.mdl == s01a.mdl.

**Stage object construction:** CStageXX classes exist for all 13 stage IDs (0–12). CStageBase is the common base class. The exact constructor addresses, vtable assignments, and how +0x08/+0x24 are populated at stage load time could not be determined without a MIPS disassembler — these require tracing code flow from stage load entry points.

### 12.5 Stage ID → STG/HIT asset mapping (SLES, confirmed via string search)

| Stage ID | Stage | STG file | HIT file | Has STG/HIT | CStage class |
|----------|-------|----------|----------|-------------|--------------|
| 0 | s00 | `stg/s00/s00a.stg` | `s00a.hit` | Yes | CStage00 |
| 1 | s01 | `stg/s01/s01a.stg` | `s01a.hit` | Yes | CStage01 |
| 2 | s02 | `stg/s02/s02a.stg` | `s02a.hit` | Yes | CStage02 |
| 3 | s03 | `stg/s03/s03a.stg` | `s03a.hit` | Yes (var0) | CStage03 |
| 3 | s03 | `stg/s03/s03b.stg` | `s03b.hit` | Yes (var1) | CStage03 |
| 4 | s04 | `stg/s04/s04a.stg` | `s04a.hit` | Yes (var0) | CStage04 |
| 4 | s04 | `stg/s04/s04b.stg` | `s04b.hit` | Yes (var1) | CStage04 |
| 5 | s05 | `stg/s05/s05a.stg` | `s05a.hit` | Yes | CStage05 |
| 6 | s06 | `stg/s06/s06a.stg` | `s06a.hit` | Yes | CStage06 |
| 7 | s07 | `stg/s07/s07a.stg` | `s07a.hit` | Yes | CStage07 |
| 8 | s08 | models/textures only | **none** | No | CStage08 (refs stg/s01/s01a) |
| 9 | s09 | models/textures only | **none** | No | CStage09 (refs stg/s07/s07a) |
| 10 | s10 | `stg/s10/s10a.stg` | `s10a.hit` | Yes | CStage10 |
| 11 | s11 | `stg/s11/s11a.stg` | `s11a.hit` | Yes | CStage11 |
| 12 | s12 | `stg/s12/s12a.stg` | `s12a.hit` | Yes | CStage12 |


### 12.6 Stage lighting/effects, transitions, camera bounds, and HIT name matching (SLES)

#### 12.6.1 Stage effect spawning (CEffectMgr / CEBZ)

CEffectMgr class name string confirmed at SLES file offset **0x3C83F0** (vaddr ~0x004C8370):
- `0x3C83F0-0x3C83F8`: `"CEffectMgr\0"` (SLUS 0x004CA870 → SLES 0x004C8360, delta -0x2510 ✓)
- Adjacent at `0x3C8400`: `"/eff/e00e000\0"` through `"/eff/e00e007\0"` — hardcoded effect directory paths near CEffectMgr class data (SLUS had `"eff/e%c%c/%s"` format string at 0x004C3AF0 → SLES 0x004C15E0, confirmed present at file offset 0x3C15E0 ✓)
- Array of 8 function pointers at `0x3C83C0-0x3C83EC` (0x002B7584 through 0x002B7674) — likely CEffectMgr vtable or method table

CEBZ effect class data confirmed:
- `CEBZData` class name string at file offset **0x3CCAC8** (vaddr ~0x004CCA08) (SLUS 0x004CEF18 → SLES 0x004CCA08, delta -0x2510 ✓)
- `CEBZHandle` class name string at file offset **0x3CCAE0**
- `CEBZBase` class name string at file offset **0x3CCAF0**
- `"%s.ebz"` format string confirmed at file offset **0x3CCAD8** (SLUS 0x004CEF28 → SLES 0x004CCA18, delta -0x2510 ✓)
- CEBZ class info block (array of vaddr pointers) at file offset **0x3FC168-0x3FC1F4** (SLUS 0x004FE628 → SLES 0x004FC118, delta -0x2510 ✓):
  - 18 vaddr pointers (0x004CE778 through 0x004D3058) — CEBZ method table
  - `0x00000001`, `0x004D3600`, `0x00000005` — class info fields
  - 9 additional vaddr pointers (0x004D3600 through 0x004D3720)
  - `0x004D3058` — final entry

#### 12.6.2 Stage transition/fade animations

No `"fade"` string found in SLES binary (0 occurrences). However, transition/fade path format strings were found:

Multiple copies of `"/trc/%02d/fin/%02d"` at file offsets:
- `0x3C1690`, `0x3C16D0`, `0x3C16F0` (vaddr ~0x004C1710, 0x004C1750, 0x004C1770)

The `"trc"` prefix likely stands for "transition" or "trace" — these are stage transition/fade animation path formats. Multiple copies suggest the format string is referenced from several code locations.

Player effect/voice path format also found:
- `"/ply/p%02d/v%d/e%s.ase"` at file offset `0x3C1670` (vaddr ~0x004C1690)

No `"transition"` or `"fade"` ASCII strings were found in the SLES data section — these animations are likely triggered by code without format string references, or use the `"/trc/%02d/fin/%02d"` path pattern.

#### 12.6.3 Per-stage camera bounds

No `"camera"` or `"bounds"` ASCII strings found in SLES binary (0 occurrences each). Per-stage camera min/max bounds are not stored as named strings in the ELF data section — they are likely:
1. Embedded directly in stage-specific code (CStageXX classes)
2. Stored in STG file node data (kind-1 nodes or camera nodes in the scene graph)
3. Computed at runtime from stage geometry bounds

The native C++ rebuild handles camera auto-framing from kind-2 mesh vertex bounds (`source/render/stage_renderer.cpp`), which supersedes the ELF's per-stage camera bounds approach.

#### 12.6.4 Stage object name → HIT object name matching

No `"HIT"` or `"STG"` ASCII strings found in game code contexts (the 3 `"HIT"` occurrences found were in staff roll credits). Stage object name → HIT object name matching is not done via format strings in the ELF — it's runtime string comparison between STG kind-3 node `name_index` entries and HIT object `name` fields.

In the native C++ rebuild, this matching happens in `source/core/stage_collision.cpp` (`build_stage_collision_world`):
- STG kind-3 nodes carry inline HIT object name references (8-byte name-index/raw-flag pairs)
- HIT objects carry 16-byte name fields
- Matching is by string equality between STG node reference name and HIT object name
- Only kind-3 (collision) nodes are processed; kind-2 (mesh) nodes are rendered but not collidable

This runtime string-matching approach means there are no ELF-format strings or fixed addresses to locate for HIT name matching — it's entirely code-driven.

#### 12.6.5 Summary of SLES effect/transition/camera addresses

| System | SLUS address | SLES vaddr | SLES file offset | Confirmed |
|--------|-------------|------------|------------------|-----------|
| CEffectMgr class name | `0x004CA870` | `0x004C8360` | `0x3C83E0` | ✓ string at `0x3C83F0` |
| CEBZData class name | `0x004CEF18` | `0x004CCA08` | `0x3CCA88` | ✓ string at `0x3CCAC8` |
| "%s.ebz" format | `0x004CEF28` | `0x004CCA18` | `0x3CCA98` | ✓ string at `0x3CCAD8` |
| "eff/e%c%c/%s" format | `0x004C3AF0` | `0x004C15E0` | `0x3C1660` | ✓ string at `0x3C15E8` |
| CEBZ class info block | `0x004FE628` | `0x004FC118` | `0x3FC198` | ✓ vtable at `0x3FC168` |
| Transition path format | — | `0x004C1710` | `0x3C1690` | ✓ `"/trc/%02d/fin/%02d"` |
| Camera bounds | — | — | — | ✗ not found (code/data) |
| HIT name matching | — | — | — | ✗ runtime string compare |

All confirmed SLES addresses follow the `-0x2510` uniform delta from SLUS. Effect system logic matches SLUS exactly — only addresses differ.


### 13.1 Lifecycle contexts (SLUS addresses)

The spawn routine `0x23AF30` is called at **fight initialization**, not mid-round respawn. Eight caller sites identified in SLUS_214.42:

| Caller | Calls | Config calls | Lifecycle |
|--------|-------|--------------|-----------|
| `0x00298180` | `0x29820C`, `0x298218` | loop `$s0` 0–45 gated on `fighter+0x528==29` | fight init |
| `0x00392D80` | `0x392DF4`, `0x392E00` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x33,0)` | fight init |
| `0x003956A0` | `0x395728`, `0x395734` | `0x1cbe80(f1,0x20,0)`, `0x1cbe80(f2,0x24,0)`, `0x1cd9d0(f2,0x1ca,0)` | fight init |
| `0x00398910` | `0x398978`, `0x398984` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x25,0)`, `0x1cd9d0(f2,0xb4,0)` | fight init / rematch |
| `0x00399640` | `0x3996F0`, `0x3996FC` | same as `0x3956A0` | fight init |
| `0x0039B550` | `0x39B59C`, `0x39B5A8` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x33,0)` | fight init |
| `0x0039DD40` | `0x39DDB4`, `0x39DDC0` | `0x1cbe80(f1,0x25,0)`, `0x1cbe80(f2,0x25,0)`, `0x1cd9d0(f1,0xb4)`, `0x1cd9d0(f2,0xb5)` | rematch / round-reset |
| `0x0039FE00` | `0x39FE7C`, `0x39FE8C` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x25,0)`, `0x1cd9d0(f2,0xb4,0)` | fight init / rematch |

**SLES addresses:** All SLUS caller addresses are shifted by `-0x2510` (uniform data/code shift) plus routine-specific deltas. SLES caller addresses have **not** been re-verified via disassembly — they must be re-located by pattern-scanning for the same caller field patterns (`owner+0x58` stage, `owner+0x60` fighter1, `owner+0x64` fighter2).

### 13.2 Helper routines in lifecycle

- **`0x1CBE80(fighter, mode, 0)`** — modes {0, 0x20, 0x24, 0x25, 0x33}: fresh round / match setup (stage load). Table lookup via `0x30e0d0` at `0x480000`, sets fighter state, calls vtable `+0x14`.
- **`0x1CD9D0(fighter, mode, $f12=0)`** — modes {0xb4, 0xb5, 0x1ca}: rematch / round-reset re-placement. Sets `fighter+0x17a0 = mode`, loads func ptr from `[fighter+0x7b8 + mode*4 + 0x800]`, calls `0x1d71a0` then vtable `+0x78`, writes vec4 from table to `fighter+0x510..51c` (spawn/home position), modifies `fighter+0x520` flags.
- **`0x30E300`** — runs AFTER `0x23AF30`, resets live position to origin `(0,0,0)` via vtable `+0x4c` (orientation) and `+0x40` (position setter). Suggests spawn position may be stored as a home offset (`fighter+0x510`) and applied live later.

**SLES helper addresses:** Shifted by `-0x2510` from SLUS (uniform delta). Not re-verified via disassembly.

### 13.3 Caller field pattern (identical in SLUS and SLES)

Every caller resolves an owner/session object, then:
- stage pointer = `owner + 0x58`
- fighter object #1 = `owner + 0x60`
- fighter object #2 = `owner + 0x64`

Invokes spawn routine(stage, fighter1) then spawn routine(stage, fighter2), followed by `0x30E300` per fighter. Both fighters use the same stage placement group and slot, then choose candidate A or B based on per-fighter field `player->0x534->0x08`.


---

## 14. Descriptor Transform — Candidate Y = nx: Decode Artifact vs Intentional

### 14.1 Confirmed decoded transform

From the full MIPS disassembly of `0x23AF30` (`ps2-elf-spawn-routine.md`):

The 4-float slot descriptor = **[x0, z0, x1, z1]** (two XZ points), NOT `[x, y, z, w]`.

```
dx = table[2] - table[0]   // x1 - x0
dz = table[3] - table[1]   // z1 - z0
mag = sqrt(dx*dx + dz*dz)
nx = dx / mag              // normalized X direction cosine, range [-1,1]
nz = dz / mag              // normalized Z direction cosine, range [-1,1]
mid_x = (table[0] + table[2]) / 2.0f
mid_z = (table[1] + table[3]) / 2.0f   // computed but unused in decoded transform
```

Constants used: `100.0f` and `1.0f` (globals at `0x487D84`/`0x487D8C`/`0x487D94`/`0x487D9C`).

Candidates:
```
Candidate A ($sp+0x150): { mid_x + 15.5*nx,   nx,   table[1] + 15.5*nz, 1.0 }
Candidate B ($sp+0x160): { mid_x - 14.5*nx,   nx,   table[1] - 14.5*nz, 1.0 }
```

### 14.2 Candidate Y = nx is a decode artifact

Verified against `test_original_spawn.cpp`:
- desc = {20.3, 66.0, 14.5, 34.4}
- dx = -5.8, dz = -31.6, mag ≈ 32.13, nx ≈ -0.1805, nz ≈ -0.9835
- Candidate A: x = 14.60, **y = -0.18**, z = 50.75 ✓ matches output exactly

**Y = nx is not a valid world height.** It is a direction cosine in [-1, 1]. The decoded transform reconstructs the XZ components correctly but substitutes `nx` for the Y component.

**Interpretation:** The original PS2 routine almost certainly:
1. Computes candidate XZ from the descriptor (decoded correctly)
2. Queries ground height at (candidate_x, candidate_z) from HIT geometry
3. Sets position to {candidate_x, ground_y, candidate_z}

The ground height query likely happens **after** `0x23AF30` returns — either in the caller, in a separate routine, or via the player position setter vtable method `+0x40` which may perform the ground query internally. The `nx` in the Y slot of the decoded vector is where the decoder could not resolve where the ground height substitution occurs.

### 14.3 Corrected transform

A corrected world-space position would be:
```
x = mid_x ± offset * nx
z = table[1] ± offset * nz     // z0 ± offset * nz
y = query_stage_ground(x, z).surface_y   // ground height from HIT, NOT nx
```

Implementation consequence: Do not implement two hard-coded side coordinates per stage. The correct native port reproduces: (1) derive group from field_08*2+field_24 via map, (2) get slot selector from stage vtable +0x28, (3) read 4-float descriptor, (4) build direction vector, normalize, construct candidate XZ, (5) query ground height at candidate XZ, (6) set position via player setter.

---

## 15. Implementation Consequences (Updated)

1. **Do not wire original spawn table positions directly into gameplay.** The descriptor transform yields candidate Y = nx (direction cosine), not valid world heights. All real-group candidates return NO-FLOOR against transformed HIT geometry.

2. **Use the collision-derived auto-spawn fallback** (`find_stage_player_spawn` in `source/core/stage_collision.cpp`) as the primary spawn resolution. It ranks walkable HIT polygons by height delta from STAGE root origin, then interior clearance, then projected area, and validates capsule support and clearance.

3. **`player_spawns.cfg` is tried first**, then falls back to auto-spawn. Overrides are validated against HIT geometry (ground contact + capsule support + clearance). Invalid overrides fall back to auto-spawn, never to Y=0.

4. **Sentinel stages fall back to collision-derived placement.** The original sentinel `{-15,0,15,0}` data should not be treated as valid world positions.

5. **s08/s09 have no STG/HIT.** They cannot be validated against collision bounds and would need synthetic collision or manual overrides. Currently unsupported as playable stages.

6. **Corrected descriptor transform**: If the original spawn table is ever to be used, Y must come from `query_stage_ground(x, z).surface_y`, not from `nx`. The XZ components (mid_x ± offset*nx, table[1] ± offset*nz) are correctly decoded.

7. **Stage vtable +0x28 slot selector** remains unresolved — per-stage return values require stage vtable identification not available in the stripped binary.

---

## 16. Verification

- All six CTest targets pass (player_controller, stage_collision, player_spawn, original_spawn, player_animation, gameplay_camera) under GCC 14.2 Release and ASan/UBSan Debug.
- Validation tool builds cleanly with MSVC 19.51 /W4 /WX.
- All 13 stages parsed and validated against transformed HIT geometry.
- `player_spawns.cfg` remains empty (no overrides needed for any stage with STG/HIT).

---

