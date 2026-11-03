# Codebase Cleanup Plan

## Goal
Clean up file structure, remove duplicates, split monolithic files, fix CMake, and update stale documentation.

## Current Issues Identified

### 1. Duplicate / redundant files
- `source/audio/adpcm.{hpp,cpp}` and `source/core/audio/adpcm.{hpp,cpp}` are two different implementations of the same decoder. `sdl_main.cpp` includes the former; `ams.cpp` includes the latter. Consolidate into one.
- `source/core/info_archive.cpp` is listed twice in CMakeLists.txt (lines 22 and 29).
- `source/audio/adpcm.hpp` has `k_coefficients` as a member static; `source/core/audio/adpcm.hpp` has a `reset()` method. Merge into the `core/audio/` version and delete the top-level `source/audio/` copy.

### 2. Monolithic files needing split

#### `source/rasterizer/` (new, low-level GL helpers)
Split from `render/renderer.cpp`:
- `rasterizer/gl_helpers.cpp` / `.hpp`: `upload_glyph_textures`, `begin_2d`, `end_2d`, `draw_glyph`, `draw_text`, `gl_mult_matrix`, `compile_static_mesh_list`.
- `rasterizer/math_helpers.cpp` / `.hpp`: `node_local_mat`, `multiply4`, `transform_point`, `build_world`, `mat4_inverse`, `skin_position` (currently anonymous namespace in renderer.cpp).

#### `source/render/renderer.cpp` (~985 lines, high-level scene rendering)
Split into:
- `render/model_renderer.cpp` / `.hpp`: Model viewer rendering (skinned mesh, animation sampling, camera framing).
- `render/stage_renderer.cpp` / `.hpp`: Stage viewer rendering (mesh placement, collision, templates).
- `render/renderer.cpp` (slimmed): `render_frame` orchestration — camera setup (projection/modelview), dispatches to model/stage renderers, calls interface overlays.

#### `source/interface/` directory structure
```
source/interface/
├── debug_menu/
│   ├── debug_menu.hpp
│   └── debug_menu.cpp
├── hud.hpp          (new, moved from render)
├── hud.cpp          (new, moved from render)
├── vfs_overlay.hpp  (new, moved from render)
└── vfs_overlay.cpp  (new, moved from render)
```

#### `source/assets/assets.cpp`
Split into:
- `assets/ffon.cpp`: `load_ffon`, `glyph_ink_bounds`.
- `assets/fpix.cpp`: `upload_texture`, `load_fpix_textures`.
- `assets/discovery.cpp`: `lower_copy`, `has_extension`, `directory_of`, `list_files`, `character_prefix`, `find_texture_for`, `find_motions_for`, `find_font_near`.

#### `source/tools/main.cpp` (455 lines)
Split per command group:
- `tools/tool_main.cpp`: argument parsing, dispatch, `print_usage`.
- `tools/elf_tool.cpp`: `command_elf`.
- `tools/info_tool.cpp`: `command_info`.
- `tools/container_tool.cpp`: `command_container`, `command_texture`.
- `tools/model_tool.cpp`: `command_model`, `command_model_obj`.
- `tools/motion_tool.cpp`: `command_motion`.
- `tools/stage_tool.cpp`: `command_stage`, `command_hit`, `command_hit_obj`.
- `tools/archive_tool.cpp`: `command_extract`, `command_rebuild`.

#### `source/platform/sdl_main.cpp` (~1400 lines)
Split into:
- `platform/app.cpp`: SDL init, window/GL context, audio device, shutdown. Contains `main()` entry point.
- `platform/input.cpp`: SDL event polling and dispatch (keyboard, mouse, gamepad). Camera orbit/pan/zoom handlers.
- `platform/vfs.cpp`: Archive VFS navigation logic (entry building, Enter/Backspace/Esc handlers, asset loading by extension).
- Shared state (loaded assets, camera, debug menu, VFS) passed via a context struct or kept in `main()` and passed by reference.

### 4. CMakeLists.txt cleanup
- Remove duplicate `info_archive.cpp` entry.
- Consolidate ADPCM: keep `source/core/audio/adpcm.cpp`, delete `source/audio/`. Update `sdl_main.cpp` include.
- Add new split source files (platform, render, rasterizer, assets, interface).
- `source/interface/debug_menu/` stays as-is (no rename).
- Consider adding a `SDBZ_BUILD_TOOL` option separate from tests.

### 3. Directory structure
```
source/
├── assets/          (ffon.cpp, fpix.cpp, discovery.cpp, assets.hpp)
├── core/            (format parsers, math, model, stage, texture, animation, info_archive, binary_reader, container, elf)
│   └── audio/       (adpcm.cpp/.hpp — consolidated)
├── interface/       (debug_menu/, hud.hpp/.cpp, vfs_overlay.hpp/.cpp, part_groups.hpp/.cpp)
├── platform/        (app.cpp, input.cpp, vfs.cpp, window.hpp/.cpp, event.hpp/.cpp, audio.hpp/.cpp)
├── rasterizer/      (gl_helpers.hpp/.cpp, math_helpers.hpp/.cpp)
├── render/          (renderer.hpp/.cpp, model_renderer.hpp/.cpp, stage_renderer.hpp/.cpp)
└── tools/           (tool_main.cpp, elf_tool.cpp, info_tool.cpp, container_tool.cpp, model_tool.cpp, motion_tool.cpp, stage_tool.cpp, archive_tool.cpp)
```

### 5. Stale documentation
- `README.md`: mentions SDL3 2D renderer and placeholder fighters; update to reflect OpenGL renderer, asset viewer, Archive VFS, debug menu.
- `BUILD_STATUS.md`: SDL3 not compiled claim is outdated (we now build with FetchContent). Update compiler matrix.
- `plan.md`: still references 2D renderer and placeholder battle scene; align with current viewer-only architecture.

### 6. Code-level cleanup
- `sdl_main.cpp` uses `std::numeric_limits` but `<limits>` include may be missing (added earlier). Verify.
- `s_draw_context` and `s_camera` stay in `render/renderer.hpp` (no move). `interface/` includes `render/renderer.hpp` for the context type. This avoids creating new dependency edges and keeps GL types in `render/`.
- `renderer.hpp` includes `SDL3/SDL.h` and `SDL3/SDL_opengl.h` for `SDL_Window*` and `GLuint`; after SDL abstraction, only `GLuint` remains (GL type, not SDL). Consider forward-declaring nothing; `GLuint` is a typedef for `unsigned int`.
- `assets.hpp` declares `s_part_group` and `part_base_name`/`apply_part_visibility` which are viewer concerns; move to `interface/` or keep in `assets/`. Recommend: move to `interface/part_groups.hpp` since they're used by the viewer, not by format parsing.
- Remove dead `attack_sound` / ASE loading code in `sdl_main.cpp` (audio not played).
- `Game` class was deleted; verify no stale references remain.
- `s_ffon_font` uses `width`/`height` (no `m_`); `s_texture_image` uses `m_width`. Inconsistent. Per naming conventions, structs don't use `m_` prefix, so `s_ffon_font` is correct and `s_texture_image` should change to `width`/`height` for consistency.

### 8. SDL abstractions (concrete wrappers)
Hide SDL types from public headers. SDL3 headers only appear in `platform/*.cpp`. These are concrete classes (not interfaces), so they use `c_` prefix without `_interface` suffix.

New files:
- `platform/window.hpp` / `window.cpp`: `c_window` wraps `SDL_Window*` + `SDL_GLContext` via PIMPL (`struct impl` defined in .cpp). Methods: `create(const char* title, int w, int h)`, `destroy()`, `get_size_pixels(int& w, int& h) const`, `swap_buffers()`, `make_gl_current()`, `set_title(const char*)`. Members use `m_` prefix (class).
- `platform/event.hpp` / `event.cpp`: `e_event_type` enum (quit, key_down, key_up, mouse_button_down, mouse_button_up, mouse_motion, mouse_wheel, gamepad_added, gamepad_removed, gamepad_button_down, gamepad_button_up), `e_key` enum (a-z, digit_0-9, space, return, escape, home, end, up, down, left, right, page_up, page_down, f1, equals, minus, kp_plus, kp_minus, n, b, e, c, t, f, and modifiers), `e_mouse_button` enum (left, right, middle), `s_event` struct with `type`, `key`, `mod`, `mouse_x`, `mouse_y`, `mouse_button`, `wheel_y` (struct, no `m_` prefix). `poll_events(c_window&, std::vector<s_event>&)` wraps `SDL_PollEvent`.
- `platform/audio.hpp` / `audio.cpp`: `c_audio_device` wraps `SDL_AudioDeviceID` + `SDL_AudioSpec`. Methods: `open(int freq, int channels)`, `close()`, `play_buffer(const std::int16_t* data, size_t count)`.

Header cleanup:
- `render/renderer.hpp`: remove `#include <SDL3/SDL.h>` and `SDL3/SDL_opengl.h`. Replace `SDL_Window*` with `c_window&` or opaque handle. Keep `GLuint` (GL type, not SDL).
- `interface/debug_menu.hpp`: replace `SDL_Event` param with `const s_event&`. Replace `SDL_Keycode` with `e_key`. Remove SDL include.
- `platform/sdl_main.cpp`: replace all direct SDL calls with wrapper methods. SDL headers only in `platform/*.cpp`.

Key mapping (in `event.cpp`):
- `SDLK_HOME` → `e_key::home`, `SDLK_END` → `e_key::end`, etc.
- `SDL_BUTTON_LEFT` → `e_mouse_button::left`, etc.
- `SDLK_0`..`SDLK_9`, `SDLK_a`..`SDLK_z` → `e_key::digit_0`..`digit_9`, `e_key::a`..`z`.

Dependencies:
- `interface/` and `render/` depend on `platform/event.hpp` and `platform/window.hpp` (opaque handle), NOT on SDL.
- `platform/` is the only module that includes SDL headers.
- `platform/input.cpp` (split from sdl_main) uses `s_event` and `c_window` internally.

Note: If abstract interfaces are needed later (e.g., for mock backends in tests), they would be named `c_window_interface`, `c_event_interface`, etc. For now, concrete wrappers suffice.

### 9. MISRA C++ compliance (coding standard)
Apply MISRA C++ 2008/2023 guidelines where practical. Full compliance conflicts with heavy use of `std::vector`, `std::string`, exceptions, and recursion in the current codebase. Recommend a pragmatic subset:

Rules to enforce:
- No `goto`, `setjmp`/`longjmp`.
- No implicit conversions between signed/unsigned or narrowing conversions.
- All `switch` statements must have a `default` case.
- No C-style casts; use `static_cast`/`reinterpret_cast` explicitly.
- No `using namespace` directives in headers.
- No function-like macros; use `constexpr`/`inline` functions.
- No recursion (replace `build_world` recursion with iterative topological sort).
- No dynamic memory in hot paths (rendering loop); pre-allocate buffers.
- Every pointer dereference must be null-checked.
- No `auto` type deduction for non-trivial types; use explicit types.
- No range-based for loops over containers that may be modified (use index-based loops).

Rules to acknowledge but defer:
- No exceptions: requires replacing `throw` with `std::optional`/error codes throughout parsers. Large scope; defer to a separate task.
- No dynamic allocation: requires pool allocators for `std::vector`/`std::string`. Massive scope; defer.
- No virtual functions: current code has no virtual functions; no action needed.

Files most affected:
- `core/model.cpp`: `build_world` uses recursion and `std::function`.
- `core/info_archive.cpp`: uses `std::vector`, `std::string`, exceptions.
- `render/renderer.cpp`: uses `std::function`, `std::unordered_map`, dynamic allocation in render loop.
- `platform/sdl_main.cpp`: uses `std::vector`, `std::string`, exceptions.

Validation: enable MISRA checking via `cppcheck --enable=all --std=c++20` or a dedicated MISRA tool (e.g., PC-lint, Helix QAC) as a CI step.

### 7. Naming conventions (project-wide)
Apply consistently across all source files:
- Structs: `s_` prefix (e.g., `s_camera`, `s_draw_context`). Members do NOT use `m_` prefix (e.g., `width`, `height`, `has_model`). Already mostly followed.
- Classes: `c_` prefix (e.g., `c_game`). Members use `m_` prefix (e.g., `m_x`, `m_health`).
- Interfaces: `c_` prefix with `_interface` suffix (e.g., `c_window_interface`, `c_event_interface`). Pure virtual, no implementation.
- Enums: `e_` prefix (e.g., `e_fighter_state`). Already followed.
- Static class member variables: `s_` prefix.
- Global variables: `g_` prefix.
- All identifiers: snake_case (already mostly followed; verify no camelCase leaks).
- Brace style: Allman (opening brace on its own line). Current code uses Allman already; verify no K&R style slipped in.
- `s_ffon_font` fields `width`/`height` stay as-is (struct, no `m_` prefix). Consistent with other structs.

Note: `s_draw_context` fields stay unprefixed (`has_model`, `model`, etc.) since it's a struct. This resolves the earlier inconsistency — structs never use `m_`, only classes do.

## Proposed Task Order

1. Consolidate ADPCM (delete `source/audio/`, keep `core/audio/`, update `sdl_main.cpp` include).
2. Fix CMakeLists duplicate `info_archive.cpp`.
3. Create SDL abstractions: `platform/window.hpp/.cpp`, `platform/event.hpp/.cpp`, `platform/audio.hpp/.cpp`. Update `renderer.hpp` and `debug_menu.hpp` to remove SDL includes.
4. Extract `rasterizer/` from `render/renderer.cpp`: move GL helpers and render math helpers to `rasterizer/gl_helpers.hpp/.cpp` and `rasterizer/math_helpers.hpp/.cpp`.
5. Split `assets.cpp` into `assets/ffon.cpp`, `assets/fpix.cpp`, `assets/discovery.cpp`.
6. Create `interface/part_groups.hpp/.cpp` with `s_part_group`, `part_base_name`, `apply_part_visibility`.
7. Split `renderer.cpp`: model + stage renderers in `render/`; HUD + VFS overlay in `interface/`.
8. Split `sdl_main.cpp` into `platform/app.cpp`, `platform/input.cpp`, `platform/vfs.cpp` using new SDL abstractions.
9. Split `tools/main.cpp` into per-command files.
10. Apply naming conventions (structs no m_, classes c_ with m_, enums e_, statics s_, globals g_, snake_case, Allman) across all source files.
11. Remove dead ASE/audio loading code in `sdl_main.cpp`.
12. Apply MISRA C++ subset: remove recursion in `build_world`, add `default` cases to all switches, replace C-style casts, add null checks, remove `using namespace`.
13. Update README.md, BUILD_STATUS.md, plan.md.
14. Run full build + tests to verify.

## Validation
- `cmake --build` clean with `-Werror` for all targets.
- `ctest` passes.
- `sdbz_game.exe` runs and Archive VFS works.

## Open Questions
- Should format parsers move from `core/` to `resources/`? (plan.md suggests yes; current code keeps them in core.) Recommend: keep in core for now, rename to `core/formats/` only if splitting. This is out of scope for this cleanup unless the implementation agent decides otherwise.
- Naming conventions resolved: structs `s_` (no `m_` on members), classes `c_` (with `m_` on members), interfaces `c_..._interface`, enums `e_`, statics `s_`, globals `g_`, snake_case, Allman braces. Apply as a dedicated pass (task 10) to reduce risk.
- SDL abstractions are concrete wrappers (`c_window`, `c_event`, `c_audio_device`), not interfaces. If abstract interfaces are needed later, name them `c_window_interface`, etc. Apply as task 3.