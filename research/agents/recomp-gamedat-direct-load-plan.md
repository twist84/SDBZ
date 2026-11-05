# Recomp GAME.DAT Direct Load Plan — SuperDragonBallZ

Master implementation plan for loading GAME.DAT data directly (bypassing the INFO.DAT/GAME.DAT archive) in the recompiled game at `C:\Dev\ps2-web-recomp\sdbz\`.

Cross-references: all `recomp-*.md` reports in `C:\Dev\SuperDragonBallZ\research\agents\` plus native C++ implementation at `C:\Dev\SuperDragonBallZ\source\`.

Read-only analysis. No source files modified.

---

## 1. Current State Summary

### Archive Loading in the Recompiled Game

The recompiled game loads all game data through the **INFO.DAT/GAME.DAT archive system**:

- **INFO.DAT** (98,304 bytes): 0x30-byte header + 1,934 records of 0x30 bytes each, obfuscated with nibble-swap + inversion + key-subtraction codec. First 16 bytes are signature `www.arika.co.jp` (byte 0 varies: `7F` EU beta, `11` EU, `C2` USA, `60` JPN) and codec key. Records contain: 32-byte CP932 filename, u32 unknown, u32 start sector, u32 sector count, u32 byte size.
- **GAME.DAT**: No archive-wide header or compression. Payloads stored directly at `sector_offset * 2048`; read `byte_size` bytes. Indexed region occupies 338,550 sectors / 693,350,400 bytes. Final 35 MiB is zero-filled and unindexed.
- **IOP decoder**: Routine at IOP 0x67AC0 (module RVA 0x2C0), ARKD_DVD loaded at IOP 0x67800. Nibble swaps at 0x67B88/0x67B8C, inversion at 0x67BBC, key subtraction at 0x67BF4. Table pointer at 0x72B04.
- **EE lookup**: At 0x1C0050 walks 0x30-byte records and reads byte sizes.

### Generated Units — No Archive Loading Code

**Zero archive loading code exists in any of the 240 generated unit files.** Confirmed negative searches for: `INFO.DAT`, `GAME.DAT`, `archive`, `container`, `sector`, `codec`, `decode_info`, `encode_info`, `swap_nibbles`, `fopen`, `fread`, `read_file`, `file_exists`, `direct_load`, `override_path`, `--debug`, `--dev`, `debug_mode`, `bypass_archive`, `.stg`, `.mdl`, `.hit`, `.pix`, `.pmt`, `.ase`, `.ams`, `.ani`, `.cht`, `INFO.DAT` memory addresses (0x015CE400 EE, 0x000BE300 IOP, decoder 0x67AC0).

The generated units are pure machine-code recompilations. They receive data via register/argument conventions ($a0 = stage object, $a1 = fighter object, etc.) and expect all data to already be in memory.

### Unregistered VA Gaps

160 KB of VA space has declared but unregistered units (no generated code):

| Missing Unit IDs | VA Range | Size |
|---|---|---|
| unit_00CA, unit_00CB, unit_00CC, unit_00CD | 0x00428000–0x00437FFF | 64 KB |
| unit_00CF, unit_00D0, unit_00D1 | 0x0043C000–0x00447FFF | 48 KB |
| unit_00E6 | 0x00498000–0x0049BFFF | 16 KB |
| unit_00F4 | 0x004CD000–0x004D3FFF | 32 KB |

### Host Application

The path `C:\Dev\ps2-web-recomp\sdbz\host\` does **not exist**. No host application source is available to analyze. The host that loads and executes the generated units is outside the scope of the generated unit files.

---

## 2. Direct Loading Options

### Option A: Host Layer — Redirect GAME.DAT LBAs to Extracted Files

**Concept:** Modify the host application's disc I/O layer to intercept GAME.DAT sector reads (LBA-based) and redirect them to extracted files on disk instead.

**Required files to modify:**
- Host disc I/O layer (file does not exist at `C:\Dev\ps2-web-recomp\sdbz\host\` — must be created or located)
- Disc sector read handler: intercept `sector * 2048` reads, map LBA to extracted file path via INFO.DAT record table, serve from disk buffer instead of GAME.DAT image

**Estimated effort:** High (requires creating host disc I/O layer from scratch)

**Risks:**
- Host application source not available — cannot verify interfaces
- Requires full INFO.DAT parsing in host layer to map LBAs to filenames
- All 1,934 archive entries must be mapped to extracted file paths
- Sector alignment/padding must be preserved for unchanged assets
- Game expects encoded INFO.DAT format; decoded data must be re-encoded or bypassed entirely

---

### Option B: ELF Layer — Patch SLES_541.61 to Read from Extracted Files

**Concept:** Patch the PS2 ELF binary (`SLES_541.61`) so the IOP/EE archive loading routines read extracted files from disk (or a host-provided buffer) instead of GAME.DAT sectors.

**Required files to modify:**
- `C:\Dev\SuperDragonBallZ\assets\original\SLES_541.61` (4,180,784 bytes, EU beta ELF)
- IOP decoder routine at 0x67AC0: patch to skip codec/decode and read decoded info from host buffer
- EE lookup routine at 0x1C0050: patch to walk records from host-provided decoded table
- GAME.DAT sector read routine: patch to redirect `sector * 2048` reads to extracted file data

**Estimated effort:** Very High (requires MIPS/COP2 reverse engineering, ELF patching, runtime testing on PS2 hardware/emulator)

**Risks:**
- All absolute code addresses from SLUS/SLES research are build-specific and non-portable
- EU beta vs USA build have different code layouts (uniform -0x2510 data shift, non-uniform code shifts)
- Must re-derive all addresses for the target build
- IOP/EE dual-processor coordination adds complexity
- No PS2 hardware/emulator testing environment confirmed
- Patching a shipping ELF is fragile and version-specific

---

### Option C: Generated Unit Layer — Patch Units to Read from Host-Provided Buffers

**Concept:** Modify generated unit `.cpp` files to accept pre-loaded data buffers from the host instead of expecting archive data in memory.

**Required files to modify:**
- `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\unit_00E1.cpp` (placement table + stage_group_map, VA 0x00484000–0x00487FFF)
- `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\unit_00E0.cpp` (adjacent base stage data, VA 0x00480000–0x00483FFF)
- All 240 unit files that reference `0x00480000` (placement table base): unit_0032, unit_0033, unit_0035, unit_0036, unit_0037, unit_0038, unit_0039, unit_003A, unit_003B, unit_003C
- Spawn routine unit_004E.cpp (VA 0x00238000–0x0023BFFF) — references placement table and stage_group_map

**Estimated effort:** Very High (all 240 units are machine-code recompilations with no symbolic names; every data reference must be individually patched)

**Risks:**
- Generated units are machine-code recompilations — no source-level structure to modify
- Every `0x480000 + N` immediate value must be individually located and patched
- Data addresses are embedded as immediates in instructions, not in a single data section
- 100+ units reference `0x00480000` with hundreds of individual references
- Unit files are regenerated from the recomp pipeline — patches would be overwritten on regeneration
- No build system or regeneration pipeline documented for modified units

---

## 3. Option Comparison

| Criterion | Option A: Host Layer | Option B: ELF Layer | Option C: Generated Units |
|---|---|---|---|
| Files exist in workspace | No (host/ missing) | Yes (SLES_541.61 ELF) | Yes (240 unit .cpp files) |
| Requires MIPS reverse engineering | No | Yes | No (but unit code is machine-generated) |
| Can patch incrementally | Yes | No (whole ELF) | Yes (per-unit) |
| Survives recomp regeneration | Yes | N/A | No (overwritten) |
| Effort estimate | High | Very High | Very High |
| Risk level | High | Very High | Very High |
| Recommended | ✅ Primary | Secondary | Not recommended |

---

## 4. Recommended Approach: Host Layer Direct Loading

### Rationale

The native C++ implementation at `C:\Dev\SuperDragonBallZ\source\platform\sdl_main.cpp` **already implements direct disk loading** of `.stg`, `.mdl`, `.hit`, `.pix`, `.fon`, `.pmt` files via `read_file()` + stem-based sibling discovery (in `source/assets/discovery.cpp`). This is the reference implementation for how direct loading should work.

The recomp's generated units have **no archive loading code** — they expect data in memory. The archive loading happens outside the units (in the IOP decoder and host runtime). Therefore, the most practical approach is to modify the **host layer** to serve extracted files directly, bypassing the archive entirely.

### Step-by-Step Implementation

#### Phase 1: Extract Archive to Disk (Prerequisite)

1. Use the native tooling to extract INFO.DAT/GAME.DAT:
   ```
   python arika_dat.py extract INFO.DAT GAME.DAT extracted/
   ```
   This produces extracted `.stg`, `.mdl`, `.hit`, `.pix`, `.pmt`, `.fon`, `.ase`, `.ams`, `.ani`, `.cht` files with their original VFS paths (e.g., `ply/p01/p01a.mdl`, `stg/s01/s01a.stg`).

2. Verify extraction was byte-for-byte identical (per `archive_and_container_notes.md` §Verification).

#### Phase 2: Host Layer — Disc I/O Redirection

3. **Create host disc I/O layer** at `C:\Dev\ps2-web-recomp\sdbz\host\disc_folder.cpp`:
   - Implement `disc_folder_read(sector, count)` that maps `sector * 2048` to an extracted file path
   - Use the decoded INFO.DAT record table to map LBA → filename
   - Serve file data from disk buffer instead of GAME.DAT image
   - Preserve sector alignment and padding for unchanged assets

4. **Implement LBA-to-file mapping:**
   - Parse decoded INFO.DAT records (0x30-byte entries at offset 0x30)
   - Build map: `start_sector → entry` for all 1,934 entries
   - For a read at sector `S`, find entry where `start_sector <= S < start_sector + sector_count`
   - Compute file offset within entry: `(S - start_sector) * 2048`
   - Read `min(count * 2048, remaining bytes)` from extracted file

5. **Handle archive bypass:**
   - Optionally skip INFO.DAT decoding entirely if all files are extracted
   - Serve files directly by path from the extracted directory
   - Fall back to GAME.DAT sector reads for any entries not found on disk

#### Phase 3: Integrate with Native Loading Pipeline

6. **Leverage existing native direct loading** in `sdl_main.cpp`:
   - `--info` and `--game` CLI args already specify archive paths
   - Auto-detection checks input dir, then `assets/original/` fallback
   - `read_file()` + `parse_stg()`/`parse_fmdl()`/`parse_hit()`/`decode_first_fpix_texture()`/`parse_rfsm()`/`load_ffon()` already load from disk
   - `find_texture_for()`, `find_motions_for()`, `find_font_near()` implement stem-based sibling discovery
   - `find_archive_entry_by_path()` matches full VFS paths (e.g., `ply/p01/p01a.mdl`)

7. **Add `--direct` or `--disc` CLI flag** (optional):
   - When set, skip archive loading entirely
   - Load all assets directly from extracted directory using native discovery functions
   - Bypass `s_archive::from_encoded_info()` and sector-based reads

#### Phase 4: Validate

8. **Byte-for-byte verification:** Compare extracted file contents against GAME.DAT sector data using `validation.json` SHA-256 records.

9. **Functional testing:** Load stages, models, motions, textures, fonts from extracted files and verify rendering matches archive-based loading.

10. **Edge case testing:** Sentinel placement groups, missing `.stg` files (s08/s09), padding preservation, overlapping extents.

---

## 5. Implementation Details

### LBA-to-File Mapping Algorithm

```
function read_sectors(start_sector, sector_count):
    for each entry in decoded_info.records:
        if start_sector >= entry.m_sector_offset and
           start_sector < entry.m_sector_offset + entry.m_sector_count:
            file_offset = (start_sector - entry.m_sector_offset) * 2048
            bytes_to_read = min(sector_count * 2048,
                                entry.m_byte_size - file_offset)
            return read_from_extracted_file(entry.m_name_cp932,
                                              file_offset, bytes_to_read)
    return error: sector not in any archive entry
```

### Native Functions to Reuse

| Function | File | Purpose |
|---|---|---|
| `read_file(path)` | `binary_reader.cpp:63` | Full file read into `vector<uint8_t>` |
| `parse_stg(data)` | `stage.cpp` | Parse .stg stage scene graph |
| `parse_fmdl(data, stem)` | `model.cpp` | Parse .mdl model container |
| `parse_hit(data)` | `stage_collision.cpp` | Parse .hit collision data |
| `decode_first_fpix_texture(data)` | `texture.cpp` | Decode .pix texture (FPIX container) |
| `load_fpix_textures(data)` | `texture.cpp` | Load all textures from .pix |
| `parse_rfsm(data)` | `spt.cpp` | Parse .pmt motion package (RFSM) |
| `load_ffon(data, font)` | `fon.cpp` | Load .fon font |
| `find_texture_for(path)` | `discovery.cpp:95` | Stem-based .pix discovery |
| `find_motions_for(path)` | `discovery.cpp:111` | Stem-based .pmt discovery |
| `find_font_near(start)` | `discovery.cpp:198` | Recursive .fon discovery |

### File Extensions and Companion Relationships

| Primary | Companions | Discovery Method |
|---|---|---|
| `.stg` | `.mdl` (same stem), `.hit` (same stem), `.pix` (same stem) | Sibling files in same directory |
| `.mdl` | `.pix` (same stem or prefix), `.pmt` (same prefix) | `find_texture_for()`, `find_motions_for()` |
| `.fon` | Standalone | `find_font_near()` recursive search |

---

## 6. Files Reference

| File | Path | Relevance |
|---|---|---|
| Native archive implementation | `source/core/info_archive.cpp` (722 lines) | INFO.DAT codec, record parsing, sector arithmetic |
| Native archive header | `source/core/info_archive.hpp` (60 lines) | `s_archive_entry`, `s_archive` structs, `k_archive_sector_size=2048` |
| Native container parser | `source/core/container.cpp` (116 lines) | FPIX/FMDL chunked container format |
| Native binary reader | `source/core/binary_reader.cpp` (120 lines) | `read_u16_le`, `read_u32_le`, `read_f32_le`, `read_file`, `write_file` |
| Native asset discovery | `source/assets/discovery.cpp` (222 lines) | Stem-based companion discovery, path utilities |
| Native main entry | `source/platform/sdl_main.cpp` (2492 lines) | CLI parsing, archive auto-detection, direct disk loading |
| Native VFS overlay | `source/interface/vfs_overlay.cpp` (62 lines) | Archive VFS browser UI |
| Native debug menu | `source/interface/debug_menu/debug_menu.cpp` (329 lines) | Debug menu with Archive VFS item |
| Archive research notes | `research/archive_and_container_notes.md` (185 lines) | INFO.DAT/GAME.DAT format, codec, FPIX/FMDL/RFSM details |
| Recomp unit index | `research/agents/recomp-unit-index.md` | 240-unit VA mapping, priority units |
| Recomp system code search | `research/agents/recomp-system-code-search.md` | Audio/physics/animation/stage patterns (none found) |
| Recomp stage load search | `research/agents/recomp-stage-load-search.md` | Stage-group map + placement table access (unit_00E1) |
| Recomp master summary | `research/agents/recomp-master-summary.md` (230 lines) | Master cross-reference of all reports |
| Recomp animation controller | `research/agents/recomp-animation-controller.md` | Not found in generated units |
| Recomp archive crossref | `research/agents/recomp-archive-crossref.md` | Archive loading not found in generated units |
| Recomp direct loading | `research/agents/recomp-direct-loading-5.md` | Direct loading not found in generated units |
| ELF (EU beta) | `assets/original/SLES_541.61` (4,180,784 bytes) | Target ELF for potential patching |
| ELF (USA, removed) | `assets/original/SLUS_214.42` (was 4,196,528 bytes) | Previous target, removed |

---

## 7. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Host application source not available | Create `host/disc_folder.cpp` from scratch; define clean interface to generated units |
| INFO.DAT codec must be bypassed or decoded | Use native `decode_info()` to decode once, then serve decoded data directly |
| Sector padding must be preserved | Read exact `byte_size` from extracted file; preserve padding for unchanged assets |
| 1,934 archive entries must be mapped | Build LBA→file map from decoded INFO.DAT records once at startup |
| Unit regeneration overwrites patches | Do not patch generated units; patch host layer instead |
| EU beta vs USA build differences | Target SLES_541.61 (EU beta) as current ELF; re-derive addresses if switching builds |
| No PS2 testing environment | Validate on host PC using native C++ implementation first |

---

## 8. Decision

**Recommended: Option A (Host Layer)** — Create `host/disc_folder.cpp` to redirect GAME.DAT sector reads to extracted files. This leverages the existing native direct-loading pipeline in `sdl_main.cpp`, avoids fragile ELF patching, and does not require modifying machine-generated unit code.

**Not recommended: Option B (ELF patching)** — Too fragile, build-specific, requires MIPS reverse engineering and PS2 testing.

**Not recommended: Option C (Generated unit patching)** — Units are machine-code recompilations; patches would be overwritten on regeneration; hundreds of individual immediate values would need patching.

*Read-only analysis. No files in ps2-web-recomp were modified.*