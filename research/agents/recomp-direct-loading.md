# Recompiled Code — Direct File Loading (Archive Bypass) Analysis

## 1. Summary

**No direct file loading code exists in any of the 240 generated EE units** (`unit_0000.cpp` through `unit_00F8.cpp`) for bypassing INFO.DAT/GAME.DAT archives. The EE code has zero references to `fopen`, `open_file`, `load_file`, `file_path`, `direct_load`, `read_file`, `file_exists`, `file_size`, `stat`, `getenv`, or command-line argument processing.

All file access in the recompiled game flows through the **host/runtime disc abstraction layer** (`ps2::disc::` API), which supports two modes:

1. **ISO image mode** — opens a `.iso` or `.img` file, reads sectors via `fseek`/`fread`
2. **Extracted disc folder mode** — mounts an extracted disc folder, reads individual files directly from the filesystem by sector offset (bypassing ISO 9660 directory walking for data, but still using ISO directory structure for file lookup)

The INFO.DAT/GAME.DAT archive codec (nibble-swap + bit inversion + key subtraction) runs entirely on the **IOP**, not the EE. The EE only sees decoded data. There is **no EE-side mechanism to bypass the archive** — archive bypass would need to happen at the host/runtime layer (disc folder mode) or IOP layer.

## 2. Host/Runtime File Loading Path

### Disc Abstraction Layer (`runtime/src/disc.cpp`, `runtime/src/disc_folder.cpp`)

| API | Signature | Purpose |
|-----|-----------|---------|
| `ps2::disc::open(path)` | `bool open(const std::string &path)` | Opens ISO image OR disc folder |
| `ps2::disc::read(sector, count, out)` | `bool read(std::uint32_t, std::uint32_t, void *)` | Reads sectors via reader callback |
| `ps2::disc::find(path)` | `std::optional<Entry> find(const std::string &path)` | ISO 9660 directory lookup |
| `ps2::disc::ready(sector, count)` | `bool ready(...)` | Probe-based readiness (web mode) |

**Disc folder mode** (`disc_folder.cpp`):
- `open_folder()` mounts an extracted disc folder
- `synthetic_layout()` creates synthetic ISO 9660 metadata if no `layout.tsv` exists
- `original_layout()` reads `layout.tsv` to preserve original file LBAs
- `s_folder::read()` reads individual files from the filesystem at `lba * 2048` offset
- Each file is a separate region in the folder's region list

**Launch paths:**
- `host/main.cpp` (ps2run): `--disc image.iso|folder`, defaults to ELF folder
- `host/game_main.cpp` (SDBZ.exe): mounts the folder containing SDBZ.exe as disc source
- `host/web.cpp`: fetches disc chunks via HTTP range requests (`ps2web_fetch`)

### Environment Variables (host-side only, NOT in EE units)

| Variable | Source | Purpose |
|----------|--------|---------|
| `PS2_CONFIG` | `host/game_main.cpp:32` | Sets ps2run.ini path beside SDBZ.exe |
| `PS2_FORCE_DEINTERLACE` | `host/game_main.cpp:33` | Video deinterlace override |
| `PS2_NO_DEINTERLACE` | `host/game_main.cpp:34` | Video deinterlace override |
| `PS2_NO_VU_THREAD` | `host/web.cpp:441` | Disables VU thread |
| `PS2_PROFILE` / `PS2_PROFILE_FROM` | `host/main.cpp:120-121` | Profiling control |
| `PS2_WATCH` | `runtime/src/memory.cpp:32` | Memory watchpoint range |

None of these are referenced in the generated EE units.

## 3. Generated Units Search Results

### 3.1 Direct File Path Loading — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bfopen\b` | No matches |
| `\bopen_file\b` | No matches |
| `\bload_file\b` | No matches |
| `\bfile_path\b` | No matches |
| `\bdirect_load\b` | No matches |
| `\bread_file\b` | No matches |
| `\bfile_exists\b` | No matches |
| `\bfile_size\b` | No matches |
| `\bstat\b` | No matches |

### 3.2 Archive Bypass Mechanisms — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bload_from_disk\b` | No matches |
| `\bload_from_file\b` | No matches |
| `\bexternal_file\b` | No matches |
| `\boverride_path\b` | No matches |
| `\bloose_file\b` | No matches |
| `\bdebug\b` (whole word) | No matches |

### 3.3 Asset Discovery — Via INFO.DAT Only

| Search term | Result |
|-------------|--------|
| `.stg` string literal | No matches |
| `.mdl` string literal | No matches |
| `.hit` string literal | No matches |
| `.pix` string literal | No matches |

Assets are discovered exclusively through the **INFO.DAT table lookup** at `unit_0030.cpp:681` (`L_001C0050`, entry 11u), which walks 48-byte records and compares filenames using `unit_0024` string comparison helpers (entries 71u, 83u, 84u, 87u, 91u). No direct filesystem enumeration exists in EE code.

### 3.4 Sector-Based Reading — Found (but via disc API, not direct file I/O)

| Pattern | Count | Meaning |
|---------|-------|---------|
| `<< 11` (sector * 2048) | 25+ units | Sector-to-byte conversion for GAME.DAT reads |
| `ps2::disc::read()` | runtime only | EE calls disc API, host reads from filesystem/ISO |

The `<< 11` shifts appear in units 0006, 0007, 0008, 0009, 000A, 000C, 000F, 0011, 0012, 0014, 0016, 0018, 0019, 0020, 0021, 0030, 008B, 00D3, 00DA, 00DB, 00DD, 00E0, 00E1, 00E7, 00E8 — but these are arithmetic operations within the EE, not direct file system calls.

### 3.5 Environment Variables / Command-Line Args — NOT FOUND IN EE

| Search term | Result |
|-------------|--------|
| `\bgetenv\b` | No matches |
| `\bPS2_\b` | No matches |
| `\bargc\b` | No matches |
| `\bargv\b` | No matches |

Environment variables and command-line arguments are processed exclusively in host code (`host/main.cpp`, `host/game_main.cpp`, `host/web.cpp`), not in the EE units.

## 4. Disc Folder Mode — The Closest Thing to "Direct Loading"

When running from an extracted disc folder (see `README_FOLDER_DISC.txt`):

```
build/native-SuperDragonBallZ/ps2run games/SuperDragonBallZ/disc/SLES_541.61 --window
```

or explicitly:

```
build/native-SuperDragonBallZ/ps2run games/SuperDragonBallZ/disc/SLES_541.61 --disc games/SuperDragonBallZ/disc --window
```

The `disc_folder.cpp` implementation:
1. Reads `.ps2-disc/layout.tsv` for original file LBAs (if present)
2. Otherwise creates synthetic ISO 9660 metadata
3. Maps each file to a region: `{lba * 2048, file_size, file_path}`
4. `s_folder::read()` seeks into individual files and reads directly

This bypasses the ISO container but **still uses ISO 9660 directory structure** for file lookup via `ps2::disc::find()`. Individual files are read directly from disk by sector offset.

## 5. Comparison: Native C++ vs. Generated EE

| Aspect | Native C++ (`source/core/`, `runtime/src/`) | Generated EE Units |
|--------|--------------------------------------------|---------------------|
| File open | `std::fopen` (disc_folder.cpp, disc.cpp) | Not present |
| File read | `std::fread` via reader callback | Not present (calls disc API) |
| Directory walking | ISO 9660 `walk()` in disc.cpp | Not present |
| INFO.DAT codec | IOP-side (ARKD_DVD) | Not present in EE |
| INFO.DAT lookup | ELF 0x1C0050 | unit_0030.cpp:681 ✓ |
| Sector * 2048 | disc_folder.cpp regions | `<< 11` in 25+ units ✓ |
| Environment variables | host code | Not present |
| Command-line args | host main() | Not present |

## 6. How to Bypass INFO.DAT/GAME.DAT Archives

The recompiled game already supports **disc folder mode** which reads files directly from disk:

1. **Extract all disc files** to a folder:
   ```
   python3 scripts/extract_disc_folder.py games/SuperDragonBallZ/disc.iso games/SuperDragonBallZ/disc
   ```

2. **Run ps2run pointing at the folder**:
   ```
   build/native-SuperDragonBallZ/ps2run games/SuperDragonBallZ/disc/SLES_541.61 --disc games/SuperDragonBallZ/disc --window
   ```

3. **SDBZ.exe** mounts its own folder automatically (see `README_GAME_EXE.txt`).

### Limitations

- Disc folder mode still uses ISO 9660 directory structure for file lookup (`ps2::disc::find()`)
- Files are read by sector offset (`lba * 2048`), not by filename directly from the EE
- The EE still goes through `ps2::disc::read()` → reader callback → `std::fread` on individual files
- No EE-side patch point exists to replace archive loading with direct file loading

### What Would Be Needed for True Direct Loading

To replace INFO.DAT/GAME.DAT with direct file loading at the EE level, you would need to:

1. **Patch `unit_0030.cpp` L_001C0050** to skip INFO.DAT table walking and return hardcoded sector/size values
2. **Patch the IOP-side ARKD_DVD codec** to pass through data unmodified (or remove IOP dependency)
3. **Or replace the `ps2::disc::read()` reader** in the host to serve files from a different source

No such patch points currently exist in the generated code.

## 7. Files Searched

- 240 EE units: `unit_0000.cpp` through `unit_00F8.cpp`
- 9 VU1 microcode units: `vu1_000.cpp` through `vu1_008.cpp`
- `host/main.cpp`, `host/game_main.cpp`, `host/web.cpp` — host file loading
- `runtime/src/disc.cpp`, `runtime/src/disc_folder.cpp` — disc abstraction layer
- `runtime/src/sdbz_bridge.cpp`, `runtime/src/memory.cpp`, `runtime/src/kernel.cpp` — runtime
- `README_FOLDER_DISC.txt`, `README_GAME_EXE.txt` — documentation
