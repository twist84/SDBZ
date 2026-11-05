# Super Dragon Ball Z Recomp — Direct File Loading Investigation

## Scope
Investigate how to load game data directly, bypassing INFO.DAT/GAME.DAT archives. Examine generated units for file I/O and host code for command-line/file-loading paths.

## Search results: generated units (`generated\SuperDragonBallZ\*.cpp`)

Searched all 240 unit files + table.cpp + vu1 files for: `fopen`, `fread`, `fwrite`, `fclose`, `fseek`, `ftell`, `opendir`, `readdir`, `chdir`, `getcwd`, `stat`, `access`, `strcpy`, `strcat`, `sprintf`, `snprintf`, `format_string`, `load_file`, `read_file`, `file_load`, `data_load`, `debug_load`, `test_load`, `dev_load`, `open`, `read`, `write`.

**Result: No file I/O functions exist in the generated units.** The generated code is flat machine-code translation — every `unit_XXXX.cpp` is `void unit_XXXX(Cpu &c, uint32_t entry, uint32_t depth)` with a `switch(entry)` dispatch over basic blocks. Memory access is via raw `ld32`/`st32`/`ld8`/`st8`/`ld16`/`st16` on register-computed addresses. There are no C library calls (fopen/fread/etc.), no string manipulation functions, no file system access, and no named file-loading functions.

The only "read" operations are `ld32`/`ld8`/`ld16`/`ld64`/`ld128` (PS2 EE register/memory load instructions), e.g. `unit_0004.cpp:995`:
```cpp
c.r[2].sd[0] = static_cast<std::int8_t>(ld8((c.r[2].w[0] + 19736u)));
```
These are CPU register-to-memory loads, not file reads. They read from compiled-in base addresses (0x004B0000, 0x004E0000, etc.) with runtime-computed offsets.

**No archive bypass, file load override, or dev-load mechanism exists** in the generated code. There is no `load_file`/`read_file`/`file_load` function, no `debug_load`/`test_load`/`dev_load`, no `strcpy`/`strcat`/`sprintf` path building, and no `fopen`/`fread`/`fclose`.

## Host code — file loading paths

### `host\main.cpp` (ps2run.exe)
- Parses command line: `<elf> [--arg]... [--disc image.iso|folder] ...`
- Sets `ps2::g_options.disc_path` to the ELF's parent folder (or `--disc` argument)
- Calls `ps2::load_elf(argv[1], entry)` then `ps2::boot(entry, args)` then `ps2::run()`
- **No direct file I/O** — all disc access goes through `ps2::load_elf` + `ps2::boot` which use `g_options.disc_path` for sector-based disc reading
- IOP boot: `IOPRP300.IMG` → SIO2MAN, MCMAN, MCSERV, LIBSD, CRI_ADXI, ARKD_DVD, PADMAN (loaded as IRX modules from disc sectors)

### `host\game_main.cpp` (SDBZ.exe)
- Locates `SYSTEM.CNF` beside the executable, resolves `BOOT2` → `SLES_541.61`
- Sets `PS2_CONFIG` env var, `g_options.disc_path = folder` (the disc folder)
- Calls `ps2::load_elf(elf.string(), entry)` then `ps2::boot(entry)` then `ps2::run()`
- **No direct file I/O** — same `ps2::load_elf` + `ps2::boot` path

### `host\web.cpp` (Emscripten/web build)
- `main()` (line 375): reads `/game/boot.txt` via `std::fopen`/`std::fread`/`std::fclose` (lines 389-398) to get boot ELF path
- `emulate()` (line 301): calls `ps2::disc::open("disc", g_disc_size, read_disc, disc_ready)` (line 303) — sector-based disc reader via `read_disc` (line 238) which uses `ps2web_fetch` (line 25, XHR/fetch with Range headers)
- Loads ELF via `ps2::load_elf("/game/core.elf", entry)` (line 306)
- Loads quicksave state via `ps2web_url_read` (line 314)
- **Disc access is sector-based chunked reading** (256 KiB chunks), not file-based

### `host\window.cpp`
- Window/input management only. No file loading.

## Runtime disc layer (`runtime\src\`)

### `disc.cpp`
- `open(path)` (line 66): `std::fopen(path.c_str(), "rb")` (line 76)
- `read(sector, count, out)` (line 121): `std::fread(out, 1, bytes, file)` (line 92)
- Sector-based reading: `read(base + lba, size / kSector, data.data())` (line 35)

### `disc_folder.cpp` (extracted folder mode)
- `~s_folder()` (line 45): `std::fclose(cached_file)`
- `read(offset, output, count)` (line 47): `_wfopen`/`std::fopen` + `std::fread` (lines 75-89)
- `open(root, size, reader, probe)` (line 312): wraps folder reads as a `Reader` callback

### `system.hpp` / `system.cpp` (load_elf / boot)
- `ps2::load_elf(path, &entry)`: reads ELF from disc path, parses ELF header, loads PT_LOAD segments
- `ps2::boot(entry, args)`: reboots IOP, loads IOP modules, transfers to EE entry point
- Both use `g_options.disc_path` for all disc access — sector/LBA based, not file-by-file

## Can INFO.DAT/GAME.DAT be bypassed?

**No bypass mechanism exists in the current code.** The data loading path is:

1. `ps2run.exe`/`SDBZ.exe` → `ps2::load_elf(SLES_541.61)` → `ps2::boot()` → EE executes SLES_541.61
2. SLES_541.61 (running on EE) → IOP loads IRX modules (SIO2MAN, etc.) → game uses SIF RPC to load files from disc
3. File access goes through the disc sector reader (`disc.cpp`/`disc_folder.cpp`), reading at LBA offsets
4. INFO.DAT/GAME.DAT are just files on the disc accessed by sector; the game's ELF code knows their LBAs and reads them directly

There is no:
- `load_file`/`read_file` override hook
- Archive bypass flag
- Direct file loading path that skips INFO.DAT/GAME.DAT
- Dev/test load function
- Command-line `--load-file` or `--data-dir` option

To bypass INFO.DAT/GAME.DAT, one would need to either:
- Patch SLES_541.61's ELF to read data from different LBAs/files (requires reverse-engineering the game's file table)
- Intercept the IOP SIF RPC file operations at the `disc.cpp`/`disc_folder.cpp` layer (sector-based, not file-based — would need LBA→file mapping)
- Replace the disc reader with a custom reader that serves different data at the same LBAs

## ELF address mapping for data access

From `table.cpp`, high-address regions map to unit files:
- `0x004B0000`–`0x004B3FFF` → `unit_00EC` (table.cpp:234) — referenced by `unit_0004.cpp:993` etc. (reads bytes at `0x004B0000 + 19736..19739`)
- `0x004E0000`–`0x004E3FFF` → `unit_00F8` (table.cpp:245) — referenced by `unit_0004.cpp:1047`, `unit_001C.cpp:2232`
- `0x004F0000`–`0x004F3FFF` → (not in table, likely overlap/alias)

These appear to be data tables placed at fixed addresses in the ELF, read via base+offset — consistent with INFO.DAT/GAME.DAT contents being loaded at fixed memory locations, but without type info they cannot be identified as archive structures specifically.

## Conclusion

No direct file loading or archive bypass mechanism exists in the recompiled game. All disc access is sector/LBA-based through `disc.cpp`/`disc_folder.cpp`, driven by `g_options.disc_path`. The generated units contain no file I/O, string manipulation, or file-loading functions — only raw `ld32`/`st32` memory operations. To bypass INFO.DAT/GAME.DAT would require patching SLES_541.61's ELF or intercepting at the sector-reader layer, neither of which is supported by existing code.
