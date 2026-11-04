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
