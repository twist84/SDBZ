# Super Dragon Ball Z — Native C++ / SDL3 Rebuild

This repository is the initial native reconstruction implementation. It is a real C++20 codebase with independently testable resource readers, archive extraction/rebuilding tools, and an optional SDL3 runtime bootstrap. It is **not yet a playable recreation**.

## Implemented

- C++20 core library with explicit little-endian binary readers and checked ranges.
- Reversible INFO.DAT byte codec and validated entry-table parser.
- GAME.DAT allocation/overlap validation and safe CP932-aware extraction.
- INFO.DAT/GAME.DAT rebuilding with replacement assets, compacted sector allocation, retained archive capacity, and preserved reserve-tail bytes.
- FPIX/FMDL chunk-directory parsing.
- XPIX/TIM2 decoding for RGBA32, indexed4, and indexed8 images, including PS2 alpha expansion and indexed CLUT handling.
- BMP texture export for inspection.
- FMDL hierarchy, materials, mesh names, DMA/VIF geometry, triangle strips, skin weights, and morph vertex decoding.
- MOT2 animation-track decoding, RFSM/L3 motion-package parsing, validated forward/reverse bone bindings, linear vector sampling, and shortest-path quaternion slerp.
- STG authored hierarchy parsing with child/sibling validation, local TRS retention, inline collision-name references, and opaque control-record preservation.
- HIT object/polygon/vertex parsing with extent validation and resource-local OBJ collision export.
- Static OBJ bind-pose export, including rigid hierarchy transforms. This is an inspection export; it does not bake animation or deform skinned meshes.
- ELF32 little-endian/MIPS header and table inspection for the supplied PS2 executable.
- Command-line tools for archive inspection, extraction, rebuilding, texture conversion, model inspection, and OBJ export.
- Optional SDL3 application bootstrap with a fixed 60 Hz simulation loop, keyboard input, an input-driven placeholder battle scene, and PIX texture preview.
- Automated core tests covering archive codec/table parsing, the 1,934-entry European-beta fixture, archive rebuilding, chunk parsing, TIM2 decoding, synthetic FMDL geometry/OBJ export, MOT2/RFSM sampling, STG/HIT parsing, and ELF inspection.

The graphical bootstrap currently uses SDL3's 2D renderer API for the placeholder scene and texture preview. It is intentionally a platform bring-up step, not the final 3D renderer. SDL3 GPU, skeletal rendering, textured stage assembly, combat, sound, and original menus remain subsequent milestones. The STG reader exposes hierarchy and TRS data but does not yet construct a runtime stage scene.

See [BUILD_STATUS.md](BUILD_STATUS.md) for the exact compiler/test matrix and the SDL3 verification limitation.

## Requirements

- CMake 3.24 or newer
- C++20 compiler (MSVC 2022+, Clang 16+, or GCC 13+ recommended)
- SDL 3.2.0 or newer for the graphical runtime

The binary tools and tests do not require SDL3.

## Build the core tools and tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSDBZ_BUILD_GAME=OFF
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Build the SDL3 runtime

With SDL3 installed and its CMake package visible:

```sh
cmake -S . -B build -DSDBZ_BUILD_GAME=ON
cmake --build build --config Release
```

Visual Studio is a multi-configuration CMake generator, so `CMAKE_BUILD_TYPE` is not used; select the configuration with `--config Release`. Windows builds define `NOMINMAX` to prevent the Windows SDK `min`/`max` macros from colliding with standard C++ library calls.

Alternatively, let CMake fetch SDL 3.2.0:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSDBZ_BUILD_GAME=ON -DSDBZ_FETCH_SDL3=ON
cmake --build build --config Release
```

The graphical executable is `sdbz_game`. The current prototype controls are A/D or left/right to move, Space/up to jump, J to attack, and Escape to exit. Passing a `.pix` file as the first argument opens the decoded TIM2 texture preview.

## Command-line tools

```sh
# Inspect the supplied PlayStation 2 executable.
sdbz_tool elf path/to/SLUS_214.42

# Decode and list an INFO.DAT index; optionally validate GAME.DAT extents.
sdbz_tool info path/to/INFO.DAT
sdbz_tool info path/to/INFO.DAT path/to/GAME.DAT

# Extract every indexed asset (refuses existing output files by default).
sdbz_tool extract path/to/INFO.DAT path/to/GAME.DAT assets/extracted
sdbz_tool extract path/to/INFO.DAT path/to/GAME.DAT assets/extracted --overwrite

# Rebuild a new archive pair from original assets plus optional replacements.
# Use '-' when no replacement directory is required.
sdbz_tool rebuild path/to/INFO.DAT path/to/GAME.DAT assets/overrides build/repacked
sdbz_tool rebuild path/to/INFO.DAT path/to/GAME.DAT - build/repacked

# Inspect FPIX/FMDL child chunks and decode a PIX texture to BMP.
sdbz_tool container path/to/character.mdl
sdbz_tool container path/to/texture.pix
sdbz_tool texture path/to/texture.pix output/texture.bmp

# Inspect FMDL mesh statistics and export its static bind pose to OBJ.
sdbz_tool model path/to/character.mdl
sdbz_tool model-obj path/to/character.mdl output/character.obj

# Inspect an RFSM motion package or standalone MOT2 clip.
sdbz_tool motion path/to/character.pmt
sdbz_tool motion path/to/idle.mot

# Inspect STG scene hierarchy and HIT collision resources.
sdbz_tool stage path/to/stage.stg
sdbz_tool hit path/to/stage.hit
sdbz_tool hit-obj path/to/stage.hit output/collision.obj
```

For a rebuilt pair, the tool writes `INFO.DAT` and `GAME.DAT` into the requested output directory and refuses to overwrite existing output files. Replacement paths are resolved relative to the replacement root using the paths stored in INFO.DAT.

## Supplied reference material and version scope

- `research/format_interaction_report.md` contains the analysis of the `SLUS_214.42` ELF and runtime relationships between resource formats.
- `research/archive_and_container_notes.md` documents the INFO.DAT codec, GAME.DAT sector table and FPIX/FMDL top-level layout.
- `research/model_format.md` documents FMDL geometry, hierarchy, materials and skeletal animation structures. Its stated sample-validation scope is the European-beta `SLES_541.61` release.
- `research/stage_format.md` and `research/audio_format.md` preserve the existing stage and audio findings.
- `tests/data/INFO.decoded.eu_beta.bin` is a decoded European-beta table fixture used to validate the common codec/table layout. It is not the North American release's INFO.DAT and must not be used as that release's runtime asset index.

The uploaded ELF is a little-endian ELF32 MIPS executable with a PS2/R5900-related flag set. The format research explicitly analyses `SLUS_214.42`; the model exporter documentation separately identifies its European-beta sample scope. Region/revision-specific offsets and asset differences must continue to be checked against the exact files used for the rebuild.

The game executable and complete `INFO.DAT`/`GAME.DAT` asset archives are not bundled in this project. Place user-owned game data in the paths supplied to the tools; the `assets/original` directory is intentionally empty.

## Design rules

1. Format parsing stays independent of SDL.
2. The archive reader retains raw CP932 filename bytes; Windows extraction converts those names with code page 932.
3. Every binary extent is validated before it is used.
4. Unknown resource fields remain preserved in source data until their meaning is established.
5. glTF/OBJ are inspection/interchange outputs, not the planned runtime model format.
6. Reverse-engineered game rules should replace provisional mechanics rather than being approximated permanently.

## Current implementation boundaries

The current CMake build discovers SDL3 when available. If it is missing, CMake still builds `sdbz_tool` and `sdbz_tests`, while reporting that the runtime target was skipped. Set `SDBZ_FETCH_SDL3=ON` to fetch SDL 3.2.0 during configuration.

Not yet implemented: runtime animation graph/state selection and full in-game animation playback, STG model-instance assembly and runtime controls, HIT collision semantics/solver, EBZ effect playback, CHT facial controllers, ASE/AMS audio playback, game-specific material/shader behavior, camera/game state reconstruction, character combat logic, AI, menus, save data, and complete game modes.
