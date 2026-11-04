@echo off
setlocal
REM Build the native runtime, tools and gameplay tests, then run every test target.

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
cd /d "%SCRIPT_DIR%"

cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DSDBZ_BUILD_TESTS=ON
if errorlevel 1 goto :build_failed

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :build_failed

ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure
if errorlevel 1 goto :test_failed

set "CORE_TEST=%BUILD_DIR%\Release\sdbz_tests.exe"
if not exist "%CORE_TEST%" set "CORE_TEST=%BUILD_DIR%\Debug\sdbz_tests.exe"
if not exist "%CORE_TEST%" set "CORE_TEST=%BUILD_DIR%\sdbz_tests.exe"
if not exist "%CORE_TEST%" goto :test_failed

if exist "%SCRIPT_DIR%assets\extracted\eff\e00\e00003.ebz" (
    "%CORE_TEST%"
    if errorlevel 1 goto :test_failed
) else (
    echo Skipping asset-backed sdbz_tests: extracted game assets are not present.
)

echo.
echo Runtime build and all native tests passed.
echo.
pause
exit /b 0

:build_failed
echo.
echo Build failed.
pause
exit /b 1

:test_failed
echo.
echo Tests failed.
pause
exit /b 1
