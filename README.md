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
