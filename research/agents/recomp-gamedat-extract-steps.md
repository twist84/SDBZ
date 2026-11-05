# Super Dragon Ball Z Recomp — GAME.DAT Extraction & Direct-Loading Steps

## Scope
Document concrete steps to extract GAME.DAT contents and serve them to the recompiled game at `C:\Dev\ps2-web-recomp\sdbz\`, bypassing the INFO.DAT/GAME.DAT archive layer. Cross-references native code (`source/core/info_archive.cpp`, `source/tools/main.cpp`) and scripts (`scripts/extract_disc_folder.py`).

## Reference: How extraction tools work

### `scripts/extract_disc_folder.py`
Extracts every file from a PS2 ISO into a folder, preserving original LBAs. Writes `.ps2-disc/layout.tsv` with columns: `PS2_DISC_LAYOUT\t1\t<total_sectors>` then `<lba>\t<size>\t<storage_path>` per file. Files are stored at their original paths; duplicate names go under `.ps2-disc/layers/<layer>/<lba>/<name>`. Usage:
```
python3 scripts/extract_disc_folder.py <game.iso> <output_folder>
```

### `source/core/info_archive.cpp` + `info_archive.hpp` (INFO.DAT decoding)
Constants (`info_archive.hpp`):
- `k_archive_sector_size = 2048` (PS2 sector size)
- `k_info_header_size = 0x30` (48-byte header)
- `k_info_record_size = 0x30` (48-byte record)

INFO.DAT layout (encoded):
- Offset 0: `0x7F` (part of signature)
- Offset 1–15: signature `www.arika.co.jp` (15 bytes)
- Header total: 0x30 bytes

`decode_info(encoded)` (`info_archive.cpp:143`):
- key = first 16 bytes of encoded
- Bytes 0–15 copied verbatim
- For i >= 16: `decoded[i] = (~swap_nibbles(encoded[i])) - key[i & 15]`

`encode_info(decoded)` (inverse):
- `inverted = ~(decoded[i] + key[i & 15])`; `encoded[i] = swap_nibbles(inverted)`

Decoded record format (`from_decoded_info`, `info_archive.cpp:228`):
- File count: `read_u32_le(decoded_info, 0x2C)` (at decoded offset 0x2C)
- Each record at `0x30 + index * 0x30`:
  - name: 32 bytes, null-terminated CP932 (at +0)
  - unknown: u32 LE (at +32)
  - sector_offset: u32 LE (at +36) — sector number in GAME.DAT
  - sector_count: u32 LE (at +40) — allocated sectors
  - byte_size: u32 LE (at +44) — actual file size

`offset() = m_sector_offset * 2048`; `allocated_size() = m_sector_count * 2048`.

`s_archive::extract_all` (`info_archive.cpp:359`):
- Opens GAME.DAT, seeks to `entry->offset()` (= sector * 2048), copies `entry->m_byte_size` bytes to `<output>/<name>` (backslashes→forward slashes, CP932→path).

### `source/tools/main.cpp` (sdbz_tool native CLI)
Commands relevant to GAME.DAT:
- `sdbz_tool info <INFO.DAT> [GAME.DAT]` — lists entries: index, sector, size, name
- `sdbz_tool extract <INFO.DAT> <GAME.DAT> <output_dir> [--overwrite]` — decodes INFO.DAT, extracts every asset from GAME.DAT to individual files
- `sdbz_tool rebuild <INFO.DAT> <GAME.DAT> <replacement_dir|-> <output_dir>` — rebuilds GAME.DAT/INFO.DAT with replacement files (or '-' to keep originals), updating sector/size fields

No `ariaka_dat.py` exists in `scripts/`. The native `sdbz_tool` is the extraction tool.

## Disc reader layer (how the recomp reads disc data)

`runtime/src/disc.cpp` / `disc_folder.cpp`:
- Sector-based reading: `fopen(path, "rb")` + `fread` at `sector * 2048`
- `disc::open("disc", size, read_disc, disc_ready)` (web) or folder reader (native)
- `g_options.disc_path` set by `main.cpp` (ps2run.exe) / `game_main.cpp` (SDBZ.exe) to the disc folder
- All disc access goes through LBA/sector reads — **not file-by-file**

## Can GAME.DAT be bypassed?

**Not directly.** The disc reader is sector/LBA-based, not file-based. The game ELF (SLES_541.61) reads INFO.DAT/GAME.DAT by sector. There is no:
- `load_file`/`read_file` override hook
- `--load-file` or `--data-dir` command-line option
- Archive bypass flag
- Direct file-serving path that skips INFO.DAT/GAME.DAT

To serve extracted GAME.DAT contents instead of the archive, you must either:

### Option A: Rebuild GAME.DAT with replacement files (recommended)
Use `sdbz_tool rebuild` to create a new GAME.DAT/INFO.DAT pair where specified assets are replaced by individual files from a replacement directory. The sector/size fields in INFO.DAT are updated automatically.

```
sdbz_tool rebuild INFO.DAT GAME.DAT <replacement_dir> <output_dir>
```
- `<replacement_dir>`: folder containing replacement files at the same relative paths as archive entries
- Use `-` to keep original assets (no replacements)
- Output: `<output_dir>/INFO.DAT` and `<output_dir>/GAME.DAT`

Then point the recomp at the rebuilt archives (place them in the disc folder).

### Option B: Intercept at the sector-reader layer
Modify `disc.cpp`/`disc_folder.cpp` to map specific LBAs to extracted files instead of reading from GAME.DAT. Requires source changes to the runtime — not just data extraction.

### Option C: Extract and place at original LBAs
Extract GAME.DAT assets to individual files, then write them back at their original GAME.DAT sector LBAs into a new GAME.DAT image (essentially what `sdbz_tool rebuild` does).

## Step-by-step guide

### Step 1: Obtain INFO.DAT and GAME.DAT
They are already present in `C:\Dev\ps2-web-recomp\sdbz\games\SuperDragonBallZ\disc\` (folder extraction from `extract_disc_folder.py`):
```
C:\Dev\ps2-web-recomp\sdbz\games\SuperDragonBallZ\disc\INFO.DAT
C:\Dev\ps2-web-recomp\sdbz\games\SuperDragonBallZ\disc\GAME.DAT
```

### Step 2: Build sdbz_tool (if not already built)
The native tools live in `C:\Dev\SuperDragonBallZ\source\`. Configure and build:
```
cmake -S C:\Dev\SuperDragonBallZ -B C:\Dev\SuperDragonBallZ\build-game -DCMAKE_BUILD_TYPE=Release -DSDBZ_BUILD_GAME=ON
cmake --build C:\Dev\SuperDragonBallZ\build-game --config Release
```
The `sdbz_tool.exe` will be in `build-game/Release/`.

**Note:** SDL3 is not installed in this environment, so the SDL3 game target cannot be built here. The tools may still build if they don't depend on SDL3; verify with the actual CMakeLists.

### Step 3: Inspect INFO.DAT contents
```
sdbz_tool info <path>\INFO.DAT <path>\GAME.DAT
```
Output shows: index, sector, byte_size, name for every asset. The `sector` column is the GAME.DAT sector number; multiply by 2048 for the byte offset.

### Step 4: Extract GAME.DAT assets to individual files
```
sdbz_tool extract <path>\INFO.DAT <path>\GAME.DAT <path>\extracted_assets [--overwrite]
```
This decodes INFO.DAT (nibble-swap + key subtraction), reads each asset from GAME.DAT at `sector * 2048` for `byte_size` bytes, and writes `<output>/<name>`.

### Step 5: (Optional) Create replacement assets
Place replacement files in a directory mirroring the archive entry paths:
```
<replacement_dir>/
  ply/p01/p01a.mdl
  ply/p01/p01a.pix
  ...
```

### Step 6: Rebuild GAME.DAT with replacements (Option A)
```
sdbz_tool rebuild <path>\INFO.DAT <path>\GAME.DAT <replacement_dir> <output_dir>
```
Output: `<output_dir>/INFO.DAT` and `<output_dir>/GAME.DAT` with updated sector/size fields. Copy these back into the disc folder:
```
copy <output_dir>\INFO.DAT  C:\Dev\ps2-web-recomp\sdbz\games\SuperDragonBallZ\disc\
copy <output_dir>\GAME.DAT  C:\Dev\ps2-web-recomp\sdbz\games\SuperDragonBallZ\disc\
```

### Step 7: Launch the recomp
Use SDBZ.exe (which successfully boots SLES_541.61, bypassing the ps2run.exe PADMAN stall):
```
cd /d C:\Dev\ps2-web-recomp\sdbz\games\SuperDragonBallZ\disc
SDBZ.exe SLES_541.61 --window
```

## ELF address mapping for data tables

From `table.cpp`, high-address regions in the ELF map to unit files:
- `0x004B0000`–`0x004B3FFF` → `unit_00EC` (table.cpp:234)
- `0x004E0000`–`0x004E3FFF` → `unit_00F8` (table.cpp:245)
- `0x00480000`–`0x004CFFFF` → `unit_00E0`–`unit_00F3` (table.cpp:223-241)

`unit_0004.cpp:993-1009` reads bytes at `0x004B0000 + 19736..19739`; `unit_001C.cpp:2232-2239` loads base `0x004E0000`/`0x004F0000` into registers then adds offsets. These appear to be data tables at fixed addresses (possibly archive contents loaded at fixed ELF addresses), but without type info they cannot be confirmed as INFO.DAT/GAME.DAT structures.

## Limitations / open issues

1. **sdbz_tool may not build here** — SDL3 is not installed (`SDL3_DIR-NOTFOUND`); verify the tools build independently.
2. **No direct file-serving path** — the disc reader is sector-based; extracted files cannot simply be "pointed at" by the recomp. Rebuilding GAME.DAT (Option A) or modifying the sector reader (Option B) is required.
3. **INFO.DAT encoding** — the nibble-swap + key-subtraction encoding must be handled by `sdbz_tool extract`/`rebuild`; manual extraction would need to reimplement `decode_info`/`encode_info`.
4. **CP932 filenames** — entry names are CP932-encoded; `path_from_cp932` converts on Windows (MultiByteToWideChar CP 932).
5. **No LBA→file redirect** — the runtime has no mechanism to serve individual files at specific LBAs; this would require source changes to `disc.cpp`/`disc_folder.cpp`.

## Summary

| Step | Action | Command |
|------|--------|---------|
| 1 | Locate archives | `games\SuperDragonBallZ\disc\INFO.DAT`, `GAME.DAT` |
| 2 | Build sdbz_tool | `cmake -S ... -B build-game ...` + `cmake --build ...` |
| 3 | Inspect | `sdbz_tool info INFO.DAT GAME.DAT` |
| 4 | Extract | `sdbz_tool extract INFO.DAT GAME.DAT extracted_assets` |
| 5 | (Optional) Prepare replacements | files in `<replacement_dir>/` matching entry paths |
| 6 | Rebuild | `sdbz_tool rebuild INFO.DAT GAME.DAT <replacement_dir> output_dir` |
| 7 | Install rebuilt archives | copy to `disc\` folder |
| 8 | Launch | `SDBZ.exe SLES_541.61 --window` from `disc\` |
