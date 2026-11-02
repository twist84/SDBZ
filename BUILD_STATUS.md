# Build and verification status

Verified in the supplied development environment on 2026-11-02.

## Native core

| Configuration | Compiler | Result |
|---|---|---|
| Debug/default | GCC 14.2.0 | Build passed; CTest passed |
| Release | Clang 17.0.0 | Build passed; CTest passed |
| ASan + UBSan | Clang 17.0.0 | Build passed; CTest passed with leak detection enabled |

The CTest suite exercises synthetic format fixtures and the checked-in European-beta INFO.DAT index fixture. The INFO.DAT fixture contains 1,934 records. The supplied `SLUS_214.42` ELF was also inspected directly by `sdbz_tool elf`.

## SDL3 runtime

The SDL3 source target is included, but it was **not compiled or run** in this environment: SDL3 is not installed, and network access prevented FetchContent from downloading SDL 3.2.0. The application bootstrap currently uses SDL3's 2D renderer API and placeholder fighters; it is not the planned SDL3 GPU 3D renderer.

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
