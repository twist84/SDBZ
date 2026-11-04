# Stage-Aware Player Runtime Plan

## Goal

When a stage is loaded, automatically create a playable player actor using `ply/p01/p01a.mdl`, its companion textures and motion packages, place the player correctly for that stage, and run the player against the stage's authored `.hit` collision geometry. The player must use a dedicated gameplay camera rather than sharing the stage/viewer camera.

This is an implementation plan, not a code change.

## Current project state

The current project already has several pieces that should be reused:

- `load_default_player_from_archive()` resolves `ply/p01/p01a.mdl` when a stage is loaded from the VFS/archive. `load_archive_model()` handles companion textures and PMT motion packages.
- `parse_stg()` and `parse_hit()` already parse the stage hierarchy and HIT resource data.
- Stage node world transforms are built in `sdl_main.cpp` and stored in `stage_worlds`.
- Stage kind-3 nodes reference named HIT objects. `stage_hit_by_names` resolves those references, and the stage renderer applies the referencing node's world transform when drawing HIT wireframes.
- The player controller currently uses a provisional flat ground plane (`m_ground_height`, initially zero) and has no stage collision query.
- The renderer currently uses one shared `s_camera`. Stage auto-framing, model auto-framing, mouse controls, player follow and camera-relative movement all operate on that shared state.
- The current projection is orthographic. Its `m_distance` is used as the positive half-height/zoom scale and is clamped to a positive range. It is not currently a signed camera-to-target distance.

The HIT parser validates resource structure and retains names, object ranges, polygon vertices, normals, flags and vectors. The object flags/vectors and some STG control records are not yet semantically verified, so they must not be assigned guessed gameplay meanings.

## Implementation phases

### 1. Make stage loading initialise a complete player session

On every successful stage load, including VFS stage selection and startup stage loading:

1. Select the stage's matching `.hit` resource from the same source as the `.stg` (archive/VFS or disk companion).
2. Build or refresh the stage's transformed collision representation from its kind-3 nodes and their named HIT references.
3. Load `ply/p01/p01a.mdl` through the existing shared archive model loader, including its `.pix` and `.pmt` companions.
4. Reset player velocity, grounded state, facing, animation state and camera-follow state for the new stage.
5. Resolve a stage-specific spawn position before enabling player control.

Stage switching must clear stale collision and player state so data from the previous stage cannot leak into the new one. A missing player asset or HIT resource should produce a clear diagnostic and a controlled fallback, not a silent spawn at Y=0.

### 2. Build a world-space collision surface from `.hit`

Use the parsed HIT polygons as the source of stage collision rather than treating the visible stage mesh as the floor.

- Resolve each stage kind-3 node's named references through `stage_hit_by_names`.
- Transform every referenced HIT polygon vertex by that node's `stage_worlds` matrix. Keep the original resource-local data intact for diagnostics.
- Recompute polygon planes/normals from transformed vertices, or transform normals with the correct inverse-transpose matrix. Do not transform normals as ordinary positions.
- Preserve polygon winding and vertex order. Support n-gons correctly; do not assume a triangle fan is valid for concave polygons.
- Build query-friendly data (bounds, plane, polygon vertices and stage/object identity) so movement does not scan every polygon every frame.
- Identify candidate walkable surfaces from the actual polygon plane and slope, then validate the result against the stage corpus. Do not assume every HIT object is a floor, and do not infer meanings for unknown object flags or vectors without reverse-engineering evidence.

Start with reliable ground/support queries, then extend the same collision representation to walls, arena limits, ledges and other blocking surfaces. The collision debug view should continue to show the authored HIT geometry.

### 3. Determine valid player placement for every stage

Remove the assumption that the ground is always at world Y=0.

Preferred spawn resolution order:

1. **Authored spawn marker:** inspect STG node names, node kinds and opaque control records, then verify likely spawn records against the original executable before treating them as authoritative.
2. **Stage-specific spawn data:** support a small explicit override table/config for stages while the original spawn semantics are being recovered.
3. **Collision-derived fallback:** find a broad, walkable HIT polygon in a sensible playable area, choose a point inside it, and place the player's capsule just above its surface. Prefer a stable candidate near the playable stage area rather than blindly using the geometric centre of the stage bounds.

Validate the candidate by checking that the player capsule is supported by the selected surface, does not begin inside another collision polygon, and lies within the stage's usable area. Stage 0 is the initial reference case, but all available STG/HIT pairs must be checked. If no trustworthy floor can be found, report that fact and use an explicit diagnostic fallback; never silently assume Y=0.

### 4. Replace the flat-ground controller with stage collision movement

Extend the controller from provisional free movement to collision-aware movement:

- Fixed-timestep movement update, independent of render frame rate.
- Ground support query under the player's collision shape.
- Gravity, jumping, landing and stable ground snapping.
- Horizontal movement constrained by walls and arena boundaries.
- Slope handling based on validated HIT surface normals.
- Re-check support after movement so the player can walk across connected polygons and fall only when leaving valid support.
- Keep the player collision shape separate from the rendered MDL bounds; tune its dimensions using the character model and verified original-game evidence.

Implement ground/support resolution first. Add wall/edge and more advanced collision response only after the stage HIT geometry has been validated in-game.

### 5. Give gameplay its own camera

Separate the stage/viewer camera from the player gameplay camera.

- Keep the existing world/viewer camera for stage inspection, auto-framing, VFS/model browsing and debug work.
- Add a player camera with its own target, yaw, pitch, follow offset, smoothing and distance/zoom controls.
- In gameplay mode, use the player camera consistently for rendering both the stage and player, so they remain in the same view. Do not render the player from one camera and the stage from another.
- Stage and model auto-framing must update only the viewer camera. They must not overwrite the player camera.
- Player movement relative to camera direction must use the player camera's yaw, not the world camera's yaw.
- Camera follow should track the player with a configurable look-ahead/height offset and smooth movement without lagging badly during fast movement.
- Preserve a way to leave gameplay camera mode and return to the independent world/viewer camera without losing its framing.

#### Signed/negative camera distance

The existing `s_camera::m_distance` is currently an orthographic projection scale. Making that value negative would invert the orthographic bounds and flip the view; it would not move the camera through its target. Do not simply remove the positive clamp.

For the player camera, separate projection scale from camera position:

- Keep orthographic zoom/scale positive if the gameplay camera remains orthographic.
- Represent camera boom/orbit distance as a separate **signed** value. Negative values place the camera on the opposite side of the target.
- Allow zoom/orbit input to cross through zero, while using a small epsilon around zero to avoid a degenerate camera position.
- If the player camera is changed to perspective, retain the same signed orbit-distance semantics and test the near-plane behaviour while crossing the target.

This provides the requested negative-distance behaviour without creating a mirrored/invalid projection by accident.

### 6. Connect player motion to PMT animation states

Once movement and camera are stable, connect the already-loaded player motion packages to controller state:

- Idle when stationary and grounded.
- Walk/run based on horizontal speed and run input.
- Jump ascent, fall and landing based on vertical velocity and grounded transitions.
- Blend or transition between states without restarting clips every frame.

Use actual clip names and timing from the loaded packages; do not hardcode assumptions about clip indices until the packages have been inspected.

### 7. Add collision and gameplay diagnostics

Add a temporary debug overlay/view that can show:

- Transformed HIT polygons and their object/node names.
- Walkable surfaces distinguished from non-walkable collision.
- Candidate and selected spawn positions.
- Player collision capsule/shape and current grounded/support polygon.
- Player world position, velocity and current movement/animation state.
- Which camera is active, including signed player-camera orbit distance and positive projection scale.

This is essential for diagnosing why a stage's spawn is wrong or why the player falls through a surface.

## Test and validation plan

### Unit tests

- HIT-to-world transform using translated, rotated and scaled STG nodes.
- Polygon plane and normal calculations after transforms.
- Point-in-polygon/support queries for triangles and n-gons.
- Walkable-slope classification, including borderline slopes.
- Spawn candidate selection and capsule-clearance checks.
- Grounding, jumping, landing, walking off an edge and wall blocking.
- Camera independence: changing world camera framing/zoom must not change player camera state.
- Signed orbit distance: positive, negative and crossing zero; projection scale must remain valid and positive.

### Stage integration tests

- Load each available stage with its matching `.hit` from disk and from VFS/archive.
- Confirm `ply/p01/p01a.mdl`, its texture and all matching motion packages load on every stage transition.
- Confirm the selected spawn is supported by that stage's transformed HIT geometry.
- Confirm the player does not begin embedded in collision, fall through the ground, or spawn at the world origin merely because a stage uses a different elevation.
- Use stage 0 as the initial reference, then explicitly validate the stages that currently fail to spawn at ground level.

### Runtime acceptance criteria

- Every supported stage automatically loads the default player and companions.
- Every supported stage places the player on a valid stage surface without relying on a global Y=0 ground plane.
- The player can walk, run, jump, land and remain within the stage collision boundaries.
- The player camera follows independently; stage auto-framing and viewer camera input do not fight it.
- Negative player-camera orbit distance is supported without flipping the orthographic projection or producing a singular view.
- The Windows SDL3 build runs through stage switching and the above checks in-game.

## Recommended implementation order

1. Inventory and visualise transformed HIT surfaces for all stages.
2. Implement collision queries and tests independently of SDL/rendering.
3. Implement spawn resolution and stage-switch reset.
4. Connect the controller to the stage collision world.
5. Split world and player camera state; implement signed player-camera orbit.
6. Connect movement states to PMT animation clips.
7. Run the complete stage-by-stage Windows runtime validation.

This order isolates the uncertain part (how HIT polygons should behave as gameplay collision) before coupling it to movement and camera code.

## Implementation status (2026-11-04)

- **Phase 1 — stage/player session lifecycle:** implemented in the supplied
  Phase 1 package. Stage changes invalidate the old player session, load the
  default VFS player with companions, build collision data and resolve a spawn.
- **Phase 2 — world-space HIT collision:** implemented in the Phase 2 package.
  World-space polygons preserve source indices, are transformed using STG world
  matrices, have planes/normals/bounds recomputed, and are indexed by an XZ
  spatial grid for ground, capsule and wall candidate queries. Concave n-gons
  and large-polygon fallback are covered by tests.
- **Phase 3 — valid player placement for every stage:** implemented in the
  player_spawn package and stage collision world. `player_spawns.cfg` provides
  validated per-stage spawn overrides (stage key, position, yaw) that are
  resolved against the transformed HIT geometry with capsule support and
  clearance checks. When no override applies, `find_stage_player_spawn()`
  selects the walkable surface closest to ground level (|Y| within a tolerance
  of Y=0), tiebreaking by largest area among ground-level surfaces and by
  smallest |Y| among elevated surfaces. A large ground-level polygon's centroid
  sits in the middle of the playable area rather than on a cliff edge or
  shoreline sliver, so the player spawns on the playable floor rather than at
  an elevated cliff, below-stage geometry, or a small spawn marker. If no
  ground-level surface passes validation, the closest elevated surface is used.
  Stage transitions refuse to spawn at Y=0 when no trustworthy floor exists,
  reporting a clear diagnostic instead. `test_player_spawn.cpp` and
  `test_stage_collision.cpp` cover override parsing, case-insensitive matching,
  edge rejection, seam-spawning spawns, capsule-size rejection, ground-level
  preference, and below-ground geometry rejection.
- **Phase 4 — stage-collision movement:** implemented in the player controller
  package. Fixed-timestep (60 Hz) movement is independent of render frame rate.
  Ground support, gravity, jump, landing and stable ground snapping query the
  transformed HIT world. Horizontal movement is blocked and slides along
  near-vertical HIT walls; slope support uses validated HIT surface normals;
  support is re-checked after movement so the player walks across connected
  polygons and falls only when leaving valid support. Overhead HIT surfaces
  stop upward movement. `test_player_controller.cpp` and `test_stage_collision.cpp`
  cover fixed-step determinism, jump buffering, edge falls, wall blocking and
  sliding, landing, and landing on an authored HIT surface rather than Y=0.
- **Phase 5 — gameplay camera:** implemented in the camera package and main
  loop. The player camera is separate from the world/viewer camera, uses a
  perspective projection and a signed orbit distance that crosses zero to the
  opposite side of its target. It follows the player with a configurable
  follow offset and smoothing; stage/model auto-framing updates only the viewer
  camera. Player input is rotated by the player camera's yaw (plus 180 degrees
  on the negative-orbit side). `H` toggles transformed HIT and player-capsule
  diagnostics. `test_camera.cpp` covers camera independence, positive/negative
  orbit distance, zero crossing and follow behavior.
- **Phase 6 — PMT animation state connection:** provisionally implemented in
  player_animation.cpp with name-based clip selection (idle/walk/run/jump/fall/
  land) and fallbacks; real PMT clip names from the original packages have not
  yet been inspected, so token matching remains provisional. Wired into the
  main loop with a landing-override hold duration.
- **Phase 7 — collision and gameplay diagnostics:** implemented through the
  debug menu, `H` toggle for transformed HIT / player-capsule wireframes, and
  console output of spawn polygon, normal and area.

Verification is limited to synthetic HIT/STG fixtures: strict GCC 14.2 Release
build and all five CTests pass; all five also pass under ASan/UBSan with leak
detection. The original full INFO.DAT/GAME.DAT stage corpus and SDL3 runtime
are unavailable in this environment, so real-stage validation and Windows
runtime testing remain outstanding. Unknown STG spawn flags and HIT object
flags/vectors are not assigned guessed semantics.
