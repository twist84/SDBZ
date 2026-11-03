@echo off
REM Simple build and test script for Super Dragon Ball Z
REM Builds project with C++23 standard and runs tests

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

REM Create build directory if it doesn't exist
if not exist "build" mkdir build
pushd build

REM Configure CMake with C++23 standard
cmake .. -DCMAKE_BUILD_TYPE=Release -DSDBZ_BUILD_TESTS=ON

REM Build the project
cmake --build . --target sdbz_tests

popd

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed! Error code: %ERRORLEVEL%
    pause
    exit /b 1
)

echo.

echo Build completed successfully!
echo Running tests...

REM Check if tests executable exists in Debug directory
if exist "build\Debug\sdbz_tests.exe" (
    echo Found sdbz_tests.exe in Debug directory, running tests...
    pushd build\Debug
    sdbz_tests.exe
    popd
) else (
    echo Error: sdbz_tests.exe not found!
    pause
    exit /b 1
)