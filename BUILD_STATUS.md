# Build and verification status

Verified in the supplied development environment on 2026-11-04.

## Native core

| Configuration | Compiler | Result |
|---|---|---|
| Debug/default | GCC 14.2.0 | Build passed; CTest passed |
| Release | Clang 17.0.0 | Build passed; CTest passed |
| ASan + UBSan | Clang 17.0.0 | Build passed; CTest passed with leak detection enabled |

The CTest suite exercises synthetic format fixtures and the checked-in European-beta INFO.DAT index fixture. The INFO.DAT fixture contains 1,934 records. The supplied `SLUS_214.42` ELF was also inspected directly by `sdbz_tool elf`.

## SDL3 runtime

The SDL3 source target is included, but it was **not compiled or run** in this environment: SDL3 is not installed, and network access prevented FetchContent from downloading SDL 3.2.0. The current runtime uses SDL3 for platform/input with a compatibility OpenGL renderer for model and stage viewing; it is not yet the planned SDL3 GPU renderer or a complete fighting-game runtime.

To build it on a machine with SDL3 3.2.0 or newer installed:

```sh
cmake -S . -B build-game -DCMAKE_BUILD_TYPE=Release -DSDBZ_BUILD_GAME=ON
cmake --build build-game --config Release
```

## Input-data limitations

The uploaded research bundle includes format documentation, source tools, and the European-beta INFO index fixture, but it does not include the complete `INFO.DAT` + `GAME.DAT` asset pair or representative raw model/stage/motion assets. The native parsers therefore have synthetic tests and documented reference layouts, but the newly ported FMDL, MOT2/RFSM, STG, and HIT readers have not been run across the original resource corpus in this environment.

The model/stage documentation and INFO fixture describe the European-beta `SLES_541.61` release. The supplied ELF is `SLUS_214.42`; verify regional/revision differences before treating any data offsets or runtime behavior as universal.

## Windows/MSVC compatibility correction

A Visual Studio 18 / MSVC 19.51 build report identified Windows `max` macro collisions with `std::numeric_limits<T>::max()` in `info_archive.cpp`, plus an integer-promotion warning in `read_u16_le`. The source now defines `NOMINMAX` and `WIN32_LEAN_AND_MEAN` for Windows builds, parenthesizes every `numeric_limits::max` call so it remains safe if the macro is present, and explicitly narrows the completed 16-bit expression before constructing `std::optional<std::uint16_t>`. The corrected project was rebuilt and tested with GCC 14.2 in this environment; an MSVC rebuild is still required to confirm the user's Visual Studio configuration.

For Visual Studio generators, omit `-DCMAKE_BUILD_TYPE=Release` because they are multi-configuration generators. `cmake --build build-game --config Release` selects Release.

## Stage-aware player runtime

The player runtime now builds world-space collision polygons from each stage's kind-3
HIT references and their transformed STG node matrices. The collision controller uses
those polygons for ground support, slope limits, landing, wall blocking/sliding,
overhead clearance and edge falls. Player spawn selection checks a walkable polygon
and the configured capsule footprint; failure is reported instead of silently using
Y=0. `player_spawns.cfg` supports validated per-stage overrides for stages where the
largest walkable HIT polygon is not the desired player start area.

The gameplay camera is now independent from the world/viewer camera. It uses a
perspective projection and signed orbit distance, including crossing zero to the
opposite side of its target. The player camera follows the actor, while stage/model
auto-framing continues to update the viewer camera only. `H` toggles transformed HIT
and player-capsule diagnostics.

PMT clip selection is state-driven when the loaded clip names identify idle, walk,
run, jump, fall or landing. It deliberately falls back rather than assuming numeric
clip indices when names are not recognisable. Player movement uses fixed substeps,
acceleration/deceleration, run, jump/gravity, camera-relative input, landing and wall
collision response.

Five CTest targets now cover the player controller, transformed HIT collision and
spawn clearance, spawn override parsing/validation, PMT name-based state selection,
and independent signed-distance camera behavior. The strict GCC 14.2 core build and
all five CTests pass on 2026-11-04. SDL3 headers/libraries are not available in this
environment, so the SDL3 game target and in-game behavior have not been compiled or
verified here. The project also lacks the complete raw INFO.DAT/GAME.DAT stage corpus,
so per-stage spawn quality and clip-name conventions still require Windows testing
against the user's full game data. Unknown STG spawn flags and HIT object flags/vectors
are not assigned guessed semantics.

## VFS model companion asset fix

Selecting an `.mdl` through the VFS now resolves its texture and motion package
companions from the same archive directory. Texture lookup prefers an exact
basename match and then uses the character-prefix fallback; motion lookup loads
all matching `.pmt` packages, sorted by archive path, matching the disk loader's
selection rules. Replacing the selected model also replaces its previous
textures and motion clips instead of retaining stale resources.

The archive lookup helpers were compiled with the strict GCC warning set and
verified against the checked-in European-beta INFO index: `ply/p01/p01a.mdl`
resolves to `ply/p01/p01a.pix`, `ply/p01/p01act.pmt`, and
`ply/p01/p01sel.pmt`. The player-controller CTest also passes. The SDL3 runtime
was not compiled here because SDL3 headers/libraries are unavailable in this
environment; a Windows build and in-game VFS test remain necessary.


## Default player auto-load on stage load

Stage load completion now resolves the exact archive path `ply/p01/p01a.mdl`
and loads it through the same VFS model pipeline used by manual model selection.
This also loads the model's companion texture and motion packages, replaces
previous player-model resources, and prepares the existing player controller.
The same behaviour runs for a stage supplied at startup and for a stage loaded
from the VFS.

The path was confirmed in the checked-in European-beta INFO index fixture
(entry 554, size 1,193,984 bytes). The strict GCC 14.2 core build and
`player_controller` CTest pass. The SDL3 game target could not be compiled here
because SDL3 headers/libraries are unavailable; Windows runtime verification is
still required.


## Stage player runtime follow-up

The stage player implementation has been tightened after the initial collision
pass:

- Capsule support samples query the complete transformed HIT collision world,
  allowing adjacent HIT triangles/quads to jointly support a player or spawn
  override across floor seams. A footprint that extends beyond the combined
  floor is rejected.
- Player movement is stepped at a fixed 60 Hz with a bounded accumulator. Jump
  press edges survive sub-frame render updates but are not carried through a fall
  to trigger unexpectedly on landing.
- WASD and left-stick movement are rotated into the gameplay camera's ground-plane
  axes. The negative-orbit side adds 180 degrees to that heading, so controls
  remain camera-relative after crossing through zero.
- The mouse pan basis uses the camera's true right/up axes. The perspective player
  camera remains separate from the orthographic world viewer; signed orbit
  distance can cross zero without making the projection scale negative.

A clean GCC 14.2 Debug configure/build completed from an empty build directory,
and all five CTest targets passed after the seam-support, fixed-step, and
camera-relative-input regression tests were added. The SDL3 application target
remains uncompiled in this environment because SDL3 headers/libraries are not
available. The complete regional stage/HIT corpus is also unavailable here, so
automatic spawn selection still needs in-game validation on every stage;
`player_spawns.cfg` provides validated per-stage coordinates when a stage needs
a manually selected starting area.

## Stage player runtime — Phase 1 session lifecycle

Stage transitions now invalidate the previous player session before any stage
resources are cleared or parsed. This disables player control and camera follow,
clears actor placement and spawn validity, resets movement/grounding/facing state,
and resets animation timing/state. The reset is also performed before resolving a
new spawn, so a missing HIT surface or failed spawn cannot retain stale state from
the previously loaded stage. Successful stage loads then rebuild the transformed
HIT collision world, load `ply/p01/p01a.mdl` through the shared archive model
loader (including `.pix` and `.pmt` companions), and resolve the player spawn.

Validation: GCC 14.2 Release core build and all five CTest targets pass. The SDL3
game target could not be configured because SDL3 is not installed in this
runtime (`SDL3_DIR-NOTFOUND`, fetching disabled); the Windows SDL3 application
still needs to be built and exercised with the full game data.

## Stage player runtime — Phase 2 world-space HIT collision

Phase 2 completes the query-ready collision representation used by stage-aware
player movement:

- Kind-3 STG nodes are resolved against named HIT objects; each source HIT vertex
  is transformed by its referencing node's full world matrix. The source HIT
  resource remains unchanged, and collision polygons retain stage-node,
  HIT-object and polygon indices for diagnostics.
- Polygon bounds, planes, normals and areas are recalculated from transformed
  world-space vertices. Newell's method handles arbitrary planar n-gons, and
  support-point tests use point-in-polygon rather than a triangle-fan assumption.
- An 8-unit XZ uniform grid indexes polygon world-space bounds. Ground queries,
  capsule support/clearance checks and swept horizontal wall checks visit only
  cells touched by the query. Polygons spanning more than 4,096 cells use a
  global fallback list so arena-sized floor surfaces do not create huge grids.
- A conservative full-scan fallback remains available for collision worlds
  assembled manually without calling `build_stage_collision_spatial_index()`.
  Call that function after manually adding/removing polygons from an indexed
  world so its broad-phase data remains synchronized.

Validation: GCC 14.2 Release build passed with the project's strict warning
settings. All five CTest targets passed. The same five tests passed in an
AddressSanitizer + UndefinedBehaviorSanitizer Debug build with leak detection.
The stage-collision tests now cover translated, rotated and non-uniformly scaled
HIT geometry, recalculated normals and bounds, concave n-gons, cell partitioning,
indexed-vs-linear query equivalence over 36 separated collision instances,
large-polygon fallback, capsule seams/clearance, overhead surfaces, wall sliding,
landing and edge falls.

Limitations: this environment does not contain the complete regional INFO.DAT /
GAME.DAT stage corpus, so no real stage could be batch-validated here. The SDL3
application target also remains uncompiled because SDL3 is not installed in this
environment. The synthetic tests validate the transformation and query code,
not the semantic correctness of every original stage's HIT object references.

## Review of updated local stage-player changes (2026-11-04)

The user-supplied project update was treated as the baseline; existing local
changes and tuning were retained. The review corrected several gameplay
issues in the collision/spawn path:

- Capsule ground support now returns the centre contact height and separately
  validates the complete sampled footprint. The previous highest-ring-point
  result could lift the player on slopes and keep the player supported when
  only one side of the capsule remained over a ledge.
- Ground support after horizontal movement is now resolved at the destination.
  The previous path-wide maximum could keep the player grounded from an
  intermediate point after the final position had left the floor.
- Capsule-vs-wall clearance now uses the wall's vertical overlap to obtain the
  capsule cross-section radius. Low barriers intersecting only the bottom or
  top hemisphere no longer get tested against the full cylinder radius.
- Automatic spawn height is measured continuously relative to the transformed
  live STAGE root origin rather than absolute world Y=0. Broad half-unit height
  buckets and unverified `Flr` / `BFlr` / `Wal` name suffixes are no longer used
  to choose a spawn surface.
- Spawn overrides match the primary stage name only, not arbitrary mesh/HIT
  strings elsewhere in the STG name table.

Validation on this review: clean GCC 14.2 Release configure/build of the native
core and all five CTest targets passed. Added regressions cover sloped-floor
height, partial-footprint ledge support, low barriers, translated STAGE origins,
and edge departure. The SDL3 frontend was not compiled because SDL3 headers and
libraries are not installed in this environment; no original regional STG/HIT
corpus was present for actual-stage spawn validation. The automatic spawn
heuristic therefore remains provisional, and `player_spawns.cfg` should be used
for stages whose authored gameplay start cannot be inferred safely.


Additional audit fixes (2026-11-04):
- Signed gameplay-camera zoom now accumulates small high-resolution wheel deltas continuously across zero; the near-zero offset is applied only when calculating the render pose.
- Gamepad camera orbit and zoom are scaled by frame delta time, preventing camera response from changing with frame rate.
- Spawn candidates now compare exact height delta from the transformed STAGE root instead of broad half-unit buckets, so a higher but larger platform cannot win solely on clearance.
- The HIT debug capsule now uses the same clamped cap radius as the collision capsule when model height is shorter than its requested diameter.
- Added regression coverage for fractional wheel zoom and close-height spawn competition.

## Player floor-contact regression fix (2026-11-04)

The reviewed controller could lose its ground contact on narrow HIT floor strips
or at small polygon seams because the runtime ground query required every point
around the capsule's projected radius to overlap valid floor geometry. That
modeled a flat-bottom cylinder rather than the controller's vertical capsule.

The runtime query now prefers the actual centreline contact, retains strict
full-footprint validation for spawn placement and wide-floor ledge detection,
and adds two constrained fallbacks: a narrow/elongated HIT polygon may support
the capsule when its centre has sufficient interior clearance, and a tiny gap
between floor polygons may be bridged only when a majority of nearby samples
resolve to one consistent support plane. The old wide-floor edge rejection
remains covered.

Added controller integration tests keep a player grounded on a narrow HIT strip
for two seconds, then jump and land on that same collision world. Added collision
tests cover a tiny inter-polygon seam while retaining the wide-floor ledge-fall
regression. GCC 14.2 Release and ASan/UBSan Debug builds pass all five CTest
targets. The complete original stage corpus and SDL3 runtime are still required
to confirm this against the user's actual arena HIT files.

## Gamepad debug-menu and VFS navigation (2026-11-04)

Added controller-only menu navigation while retaining all keyboard input:

- Select/Back + Start opens or closes the debug menu. The chord is edge-latched
  until both buttons are released, so holding one button and tapping the other
  cannot repeatedly toggle the menu.
- D-pad up/down moves the selected row. D-pad left/right changes the selected
  Stage or Motion value, or sets visibility toggles off/on.
- A/South activates the selected debug-menu row or opens the selected VFS entry.
- B/East backs out one debug-menu level or VFS directory; at the root it closes
  the active menu/browser.
- In the VFS browser, D-pad left/right goes to the parent directory / opens the
  selected directory, while up/down changes the entry selection.
- The overlay prompts now display the controller controls alongside keyboard
  equivalents. A guard also handles the frame in which a menu is first opened
  before its row list has been rebuilt.

The native core Release build and all five CTest targets pass. The SDL3 game
frontend could not be compiled in this environment because SDL3 is not installed
and network access is unavailable for the CMake FetchContent download. Runtime
controller testing remains to be done on the Windows build.


## Sloped-surface grounding and jump fix (2026-11-04)

The slope-contact regression had two causes. First, capsule support required
adjacent HIT polygons to have a normal dot product of at least 0.80 and used a
fixed 0.16-unit height tolerance. A walkable ramp meeting a flat floor at a
continuous crease could therefore lose grounded state even though all sampled
points remained over walkable collision. The ring samples now allow the height
variation implied by the configured maximum walkable slope over their radius;
each sample must still resolve to a walkable polygon, so ledge detection remains
active. The fixed normal-dot cutoff was removed.

Second, surfaces above 50 degrees were classified as walls. The default
walkable limit is now 60 degrees, and the player step-up allowance is 0.42
world units, sufficient for the 14-unit/second run speed at 60 Hz on a
60-degree ramp. Near-vertical HIT faces remain blocking walls. During a jump,
if residual uphill momentum would push the destination capsule into the ramp,
horizontal movement is held for that tick while upward movement continues. A
real overhead obstruction still blocks the current vertical column and stops
the jump.

Regression tests first confirmed that the previous code failed the ramp-to-flat
crease case (loss of support, inability to cross, and inability to jump). They
now cover the crease, movement onto the adjacent floor, jumping at the crease,
and running/jumping on a 60-degree ramp at the configured maximum slope,
including the initial uphill-momentum frames after jump input. All five Release CTest targets and
all five ASan/UBSan Debug CTest targets pass. The SDL3 runtime and original
regional stage corpus remain unavailable here, so the actual arena still needs
in-game verification.
