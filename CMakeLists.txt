cmake_minimum_required(VERSION 3.24)

project(SuperDragonBallZRebuild
    VERSION 0.1.0
    DESCRIPTION "Native C++ reconstruction of Super Dragon Ball Z using SDL3"
    LANGUAGES CXX
)

option(SDBZ_BUILD_GAME "Build the SDL3 game runtime when SDL3 is available" ON)
option(SDBZ_FETCH_SDL3 "Fetch SDL3 when no system SDL3 package is available" OFF)
option(SDBZ_BUILD_TESTS "Build the native format tests" ON)

set(CMAKE_CXX_STANDARD 20)
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
    source/core/info_archive.cpp
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
    target_compile_options(sdbz_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(sdbz_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
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
        FetchContent_MakeAvailable(SDL3)
    else()
        find_package(SDL3 CONFIG QUIET)
    endif()

    if(TARGET SDL3::SDL3)
        add_executable(sdbz_game
            source/platform/sdl_main.cpp
            source/assets/assets.cpp
            source/render/renderer.cpp
        )
        target_link_libraries(sdbz_game PRIVATE sdbz_core SDL3::SDL3)
        if(WIN32)
            target_link_libraries(sdbz_game PRIVATE opengl32)
        endif()
        if(MSVC)
            target_compile_options(sdbz_game PRIVATE /W4 /permissive- /EHsc)
        else()
            target_compile_options(sdbz_game PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    else()
        message(WARNING
            "SDL3 was not found. Building sdbz_tool and sdbz_tests only. "
            "Install SDL3 or configure with -DSDBZ_FETCH_SDL3=ON to build sdbz_game."
        )
    endif()
endif()
