cmake_minimum_required(VERSION 3.24)

project(SuperDragonBallZRebuild
    VERSION 0.1.0
    DESCRIPTION "Native C++ reconstruction of Super Dragon Ball Z using SDL3"
    LANGUAGES CXX
)

option(SDBZ_BUILD_GAME "Build the SDL3 game runtime when SDL3 is available" ON)
option(SDBZ_FETCH_SDL3 "Fetch SDL3 when no system SDL3 package is available" ON)
option(SDBZ_BUILD_TESTS "Build the native format tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
add_library(sdbz_core STATIC
    source/core/binary_reader.cpp
    source/core/info_archive.cpp
    source/core/container.cpp
    source/core/elf.cpp
    source/core/math.cpp
    source/core/texture.cpp
    source/core/model.cpp
    source/core/animation.cpp
    source/core/stage.cpp
    source/core/ebz.cpp
    source/core/ase.cpp
    source/core/ams.cpp
    source/core/fon.cpp
    source/core/cht.cpp
    source/core/spt.cpp
    source/core/audio/adpcm.cpp
)
target_include_directories(sdbz_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

if(MSVC)
    target_compile_options(sdbz_core PRIVATE /W4 /permissive- /EHsc /WX)
else()
    target_compile_options(sdbz_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

add_executable(sdbz_tool
    source/tools/main.cpp
)
target_link_libraries(sdbz_tool PRIVATE sdbz_core)

if(SDBZ_BUILD_TESTS)
    enable_testing()
    add_executable(sdbz_tests
        tests/test_main.cpp
    )
    target_link_libraries(sdbz_tests PRIVATE sdbz_core)
    target_compile_definitions(sdbz_tests PRIVATE
        SDBZ_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    )
endif()

if(SDBZ_BUILD_GAME)
    if(SDBZ_FETCH_SDL3)
        include(FetchContent)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.0
            GIT_SHALLOW TRUE
        )
        set(SDL_SHARED ON CACHE BOOL "Build SDL3 shared library" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "Build SDL3 static library" FORCE)
        set(SDL_VIDEO ON CACHE BOOL "Enable the Video subsystem" FORCE)
        set(SDL_DIRECTX ON CACHE BOOL "Use DirectX for Windows audio/video" FORCE)
        FetchContent_MakeAvailable(SDL3)
    else()
        find_package(SDL3 CONFIG QUIET)
    endif()

    if(TARGET SDL3::SDL3)
        add_executable(sdbz_game
            source/platform/sdl_main.cpp
            source/platform/window.cpp
            source/platform/event.cpp
            source/platform/audio.cpp
            source/assets/ffon.cpp
            source/assets/fpix.cpp
            source/assets/discovery.cpp
            source/interface/part_groups.cpp
            source/interface/debug_menu/debug_menu.cpp
            source/rasterizer/c_rasterizer.cpp
            source/rasterizer/math_helpers.cpp
            source/rasterizer/gl_helpers.cpp
            source/render/model_renderer.cpp
            source/render/stage_renderer.cpp
            source/render/renderer.cpp
            source/interface/hud.cpp
            source/interface/vfs_overlay.cpp
        )
        target_link_libraries(sdbz_game PRIVATE sdbz_core SDL3::SDL3)
        if(WIN32)
            target_link_libraries(sdbz_game PRIVATE opengl32)
        endif()
        if(MSVC)
            target_compile_options(sdbz_game PRIVATE /W4 /permissive- /EHsc /WX)
        else()
            target_compile_options(sdbz_game PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()
        # Copy the SDL3 shared library DLL next to the executable so the game
        # can find it at runtime in both Debug and Release builds.
        if(WIN32 AND TARGET SDL3::SDL3)
            add_custom_command(TARGET sdbz_game POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:SDL3::SDL3>"
                    "$<TARGET_FILE_DIR:sdbz_game>")
        endif()

        # Make sdbz_game the Visual Studio startup project and debug from the
        # source root so relative asset paths resolve correctly.
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT sdbz_game)
        set_target_properties(sdbz_game PROPERTIES
            DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    else()
        message(WARNING
            "SDL3 was not found. Building sdbz_tool and sdbz_tests only. "
            "Install SDL3 or configure with -DSDBZ_FETCH_SDL3=ON to build sdbz_game."
        )
    endif()
endif()
