@echo off
REM Super Dragon Ball Z Build and Run Script
REM This script builds the project and provides options to run the game or tools

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

REM Run build_and_test.bat to build the project
call build_and_test.bat

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed! Error code: %ERRORLEVEL%
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo.

REM Show build options
(
    echo Build Options:
    echo 1. Run the game (sdbz_game)
    echo 2. Run native tools (sdbz_tool)
    echo 3. Run tests (sdbz_tests)
    echo 4. Rebuild everything
    echo 5. Exit
) > con

:menu
echo Build Options:
echo 1. Run the game (sdbz_game)
echo 2. Run native tools (sdbz_tool)
echo 3. Run tests (sdbz_tests)
echo 4. Rebuild everything
echo 5. Exit
set /p choice=Select an option (1-5): 

if "%choice%"=="1" goto :opt_game
if "%choice%"=="2" goto :opt_tools
if "%choice%"=="3" goto :opt_tests
if "%choice%"=="4" goto :opt_rebuild
if "%choice%"=="5" goto :opt_exit
echo Invalid option. Please select 1-5.
goto :menu

:opt_game
echo Running game...
if exist "build\Release\sdbz_game.exe" (
    "build\Release\sdbz_game.exe"
) else if exist "build\Debug\sdbz_game.exe" (
    "build\Debug\sdbz_game.exe"
) else if exist "build\sdbz_game.exe" (
    "build\sdbz_game.exe"
) else (
    echo Error: Game executable not found!
    echo Make sure the game was built.
    pause
)
goto :menu

:opt_tools
echo Running native tools...
if exist "build\Release\sdbz_tool.exe" (
    "build\Release\sdbz_tool.exe" %*
) else if exist "build\Debug\sdbz_tool.exe" (
    "build\Debug\sdbz_tool.exe" %*
) else if exist "build\sdbz_tool.exe" (
    "build\sdbz_tool.exe" %*
) else (
    echo Error: Native tools executable not found!
    pause
)
goto :menu

:opt_tests
echo Running tests...
if exist "build\Release\sdbz_tests.exe" (
    "build\Release\sdbz_tests.exe"
) else if exist "build\Debug\sdbz_tests.exe" (
    "build\Debug\sdbz_tests.exe"
) else if exist "build\sdbz_tests.exe" (
    "build\sdbz_tests.exe"
) else (
    echo Error: Tests executable not found!
    pause
)
goto :menu

:opt_rebuild
echo Rebuilding everything...
rmdir /s /q build
call "%SCRIPT_DIR%"build_and_test.bat
if %ERRORLEVEL% NEQ 0 (
    echo Rebuild failed!
    pause
) else (
    echo Rebuild completed successfully!
)
goto :menu

:opt_exit
echo Exiting...
exit /b 0