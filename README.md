# Super Dragon Ball Z Build and Run Scripts

This directory contains batch scripts to build and run the Super Dragon Ball Z project.

## Scripts

### build_and_run.bat
Main script that builds the project and provides a menu to run the game, native tools, or tests.

### build_and_test.bat
Script to build the project with C++23 standard and test the native format tests.

## build_and_run.bat - Usage

**Initial Setup:**
```
build_and_run.bat
```

This script will:
1. Configure CMake to use C++23 standard
2. Build all targets (game, native tools, tests)
3. Display a menu with options

**Menu Options:**
- **1. Run the game** - Launches the Super Dragon Ball Z game
- **2. Run native tools** - Launches the native format tools (sdbz_tool)
- **3. Run tests** - Runs the native format tests (sdbz_tests)
- **4. Rebuild everything** - Cleans and rebuilds the entire project
- **5. Exit** - Exits the script

## build_and_test.bat - Usage

**To build with testing:**
```
build_and_test.bat
```

This script will:
1. Configure CMake to use C++23 standard
2. Build all targets
3. Run the native format tests

## Prerequisites

- CMake 3.24 or higher
- Visual Studio 2019 or newer (for Windows)
- SDL3 development libraries (optional, will be fetched automatically)
- OpenGL libraries (Windows)

## Build Options

You can pass additional CMake options when configuring:
```
cmake -S . -B build [OPTIONS]
```

Common options:
- `-DSDBZ_BUILD_GAME=ON/OFF` - Build the SDL3 game runtime (default: ON)
- `-DSDBZ_FETCH_SDL3=ON/OFF` - Fetch SDL3 automatically (default: ON)
- `-DSDBZ_BUILD_TESTS=ON/OFF` - Build native format tests (default: ON)

## Build Outputs

After building, you'll find executables in:
- `build/Release/` - Release builds
- `build/Debug/` - Debug builds

## Executable Locations

- **Game:** `build/[Release|Debug]/sdbz_game.exe`
- **Native Tools:** `build/[Release|Debug]/sdbz_tool.exe`
- **Tests:** `build/[Release|Debug]/sdbz_tests.exe`

## Troubleshooting

### Build fails due to C++ standard
Ensure CMakeLists.txt has `CMAKE_CXX_STANDARD 23` set correctly.

### SDL3 not found
The build will automatically fetch SDL3 from GitHub if `SDBZ_FETCH_SDL3` is ON (default).

### Access denied
Make sure you have permissions to create files in the project directory.

## Support

For issues or questions about building and running the project, refer to the source code comments or the original repository documentation.

## Stage-aware player runtime

After a stage finishes loading, the runtime loads `ply/p01/p01a.mdl` and its
matching `.pix` texture and `.pmt` motion packages from the archive. It builds a
world-space collision set from the stage's kind-3 HIT references and transforms
those polygons using the stage node world matrices. The player is placed on a
walkable HIT polygon with enough room for the configured collision capsule; the
runtime does not silently fall back to world Y=0 when no valid floor exists.

Press **P** to enable or disable player control after a valid stage spawn has been
found. Controls:

| Input | Action |
|---|---|
| W / A / S / D | Camera-relative movement |
| Left Shift / Right Shift | Run |
| Space | Jump |
| P | Enable / disable player control |
| C | Re-centre the gameplay camera on the player |
| Left mouse drag | Orbit the active camera |
| Right / middle mouse drag | Pan viewer camera, or adjust player-camera follow offset |
| Mouse wheel / + / - | Zoom the active camera; gameplay orbit distance can cross zero |
| H | Toggle transformed HIT polygons and player capsule diagnostics |
| Gamepad left stick | Move |
| Gamepad left shoulder | Run |
| Gamepad South (A on Xbox), outside menus | Jump |
| Gamepad Select + Start | Open / close the debug menu (or close the VFS browser) |
| Gamepad D-pad up / down | Move through debug-menu or VFS entries |
| Gamepad D-pad left / right | Change the selected debug-menu value; in VFS, go to parent / open entry |
| Gamepad South (A on Xbox), while a menu is open | Select a debug-menu or VFS entry |
| Gamepad East (B on Xbox) | Go back one menu / VFS directory level; close at the root |

The debug menu and archive VFS browser are fully navigable without a keyboard.
In the debug menu, D-pad left/right selects the previous/next stage or motion
clip, and sets the selected visibility toggle off/on. In the VFS browser,
D-pad left/right goes to the parent directory / opens the selected directory or
file. Keyboard navigation remains available.

The gameplay camera is independent of the stage viewer camera. It uses perspective
projection and a signed orbit distance; a negative value moves the camera to the
opposite side of its target without inverting the projection. The viewer retains
its own orthographic framing and zoom.

The player controller includes fixed 60 Hz movement, acceleration/deceleration,
run, jump, gravity, support/landing queries, walkable-slope checks, wall blocking,
wall sliding, edge falls and overhead collision checks. Capsule support is sampled
against the complete HIT world, so adjacent floor polygons can jointly support the
player and per-stage spawn overrides at their shared seams. Movement input follows
the gameplay camera's ground-plane axes, including after its signed orbit distance
crosses zero. PMT clips are selected by
recognisable clip names for idle, walk, run, jump, fall and landing states. If a
package does not contain recognisable names, the runtime keeps the current clip
rather than guessing clip indices.

### Stage-specific spawn overrides

`player_spawns.cfg` is an optional per-stage override file. Use a stage name from
the STG name table as the key (case-insensitive):

```text
# stage_key player_root_x player_root_y player_root_z yaw_degrees
STAGE_A 0.0 1.05 0.0 90.0
```

The configured point is checked against the stage's transformed HIT geometry,
including support height, capsule clearance and edge clearance. An invalid entry
is ignored and the collision-derived spawn finder is used instead. The fallback
chooses a large walkable HIT polygon with a clear capsule footprint; stages whose
HIT data does not contain a valid support surface are reported and player control
remains disabled.

## VFS model companion assets

Selecting a character `.mdl` from the virtual file system now performs the same
companion-file discovery as loading the model from disk. It automatically loads
an exact sibling `.pix` texture (with the same character-prefix fallback) and
merges matching sibling `.pmt` motion packages. Companion lookup is confined to
the model's archive directory, so assets from other characters or folders are
not selected accidentally.


## Default player on stage load

When a stage is loaded from disk or selected through the VFS, the runtime also
loads `ply/p01/p01a.mdl` from the archive as the player model. It uses the VFS
companion-asset loader, so the matching `.pix` texture and `.pmt` motion packages
are loaded with it. The player controller remains opt-in with **P**.

### Stage player initialisation

Every successful stage load initializes a fresh player session. The runtime
loads `ply/p01/p01a.mdl` and its matching texture/motion companions, builds the
stage's transformed `.hit` collision world, and resolves a spawn on a valid
walkable surface. Stage transitions clear the previous controller, animation,
spawn and camera-follow state before loading the new stage. Player control is
left disabled after a transition and can be enabled with `P` once a valid player
and spawn are available.

### Stage collision queries (Phase 2)

The player collision world is built from the transformed HIT polygons referenced
by STG kind-3 nodes. World-space vertices, planes, normals and bounds are
recomputed after applying each node's world matrix. Arbitrary planar n-gons are
queried as polygons, including concave shapes; their source HIT data is left
untouched for diagnostics.

The collision world builds an 8-unit XZ spatial grid used by ground, capsule and
horizontal wall queries. Very large polygons are kept in a global fallback list
instead of being expanded into thousands of cells. Collision worlds modified by
hand must have `build_stage_collision_spatial_index()` called after polygon
changes; worlds created by `build_stage_collision_world()` are indexed
automatically.

Phase 2 has passed the five native CTest targets in both strict Release and
AddressSanitizer/UndefinedBehaviorSanitizer Debug builds. The complete original
stage archive is not present in this development package, so testing against all
real stage/HIT pairs remains a runtime validation task.
