# Recomp GAME.DAT Redirect — Archive-to-File Redirection Analysis

## Search scope

Read runtime source files and searched generated units for GAME.DAT redirection pathways.

Files read:
- `C:\Dev\ps2-web-recomp\runtime\src\disc_folder.cpp` (321 lines)
- `C:\Dev\ps2-web-recomp\runtime\src\disc.cpp` (149 lines)
- `C:\Dev\ps2-web-recomp\runtime\include\ps2\disc.hpp`
- `C:\Dev\SuperDragonBallZ\source\core\info_archive.cpp` (722 lines)
- `C:\Dev\SuperDragonBallZ\source\core\binary_reader.cpp` (120 lines)
- `C:\Dev\SuperDragonBallZ\source\core\container.cpp` (116 lines)

Generated units searched:
- All `unit_*.cpp` under `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`

## 1. How folder-based disc reading works (disc_folder.cpp)

**`s_region` struct (line 29-36):** Maps a byte offset range to a single file path or in-memory buffer:
```cpp
struct s_region {
    std::uint64_t offset = 0;   // byte offset in synthetic disc
    std::uint64_t size = 0;     // byte size
    fs::path path;              // source file path (empty if in-memory)
    std::vector<std::uint8_t> memory; // in-memory data (empty if file-backed)
};
```

**`s_folder::read()` (line 47-96):** Given a byte offset and count, zero-fills output, then walks `s_region` entries that overlap the requested range, seeking each source file and reading the overlapping portion. Supports both file-backed regions (fopen/fseek/fread) and in-memory regions.

**Two layout modes:**
- **`synthetic_layout()` (line 166):** Walks the folder recursively, assigns synthetic LBAs sequentially starting after ISO 9660 metadata (PVD at sector 16, path tables, directory records). All files get contiguous synthetic LBAs. Does NOT preserve original disc LBAs.
- **`original_layout()` (line 261):** Reads `.ps2-disc/layout.tsv` (tab-separated: `LBA\tSIZE\tRELATIVE_PATH`), preserving original file LBAs exactly. Each entry becomes an `s_region` at `lba * kSector` with the file's actual size.

**`open_folder()` (line 300):** Tries `.ps2-disc/layout.tsv` first; falls back to `synthetic_layout()`. Validates regions don't overlap. Reads PVD at synthetic sector 16 to verify ISO 9660 descriptor. Calls `disc::open()` with a reader lambda that delegates to `folder->read()`.

## 2. How ISO sector reading works (disc.cpp)

**`index_layer()` (line 58):** Reads PVD at a given sector, validates "CD001" magic, calls `walk()` with root directory LBA/size from PVD fields at offsets 158/166.

**`walk()` (line 32):** Reads a directory extent sector-by-sector. Parses ISO 9660 directory records (variable length at `data[0]`, name at `rec[33]` with length `rec[32]`, LBA at `rec[2]` (4 bytes), size at `rec[10]` (4 bytes), directory flag at `rec[25] & 2`). Builds `g_index` map: uppercased path → `Entry{sector, size, directory}`. Recurses into subdirectories.

**`open()` with file path (line 66):** Opens ISO image via fopen, gets size, calls `open(name, size, reader)` with a fseek/fread reader lambda.

**`open()` generic (line 96):** Sets up `g_reader`, `g_probe`, `g_size`. Calls `index_layer(16, 0)` for layer 0, then scans sectors `layer0..layer0+64` for a second "CD001" PVD (layer 1). Builds `g_index` from both layers.

**`read()` (line 121):** Converts sector → byte offset (`sector * kSector`), calls `g_reader`, zero-fills short reads.

**`find()` (line 133):** Case-insensitive path lookup in `g_index`, strips `:` drive letters and `;1` version suffixes.

## 3. Generated units reading from high addresses (0x004B0000+)

**Finding: Units read from data section bases, NOT GAME.DAT sectors directly.** The high addresses (0x004B0000–0x005F0000) are VA base addresses for ELF data sections used for table lookups — not GAME.DAT sector reads.

Units with the most references to 0x00500000–0x005F0000 range:
- `unit_0003.cpp` — 50+ references to 0x00500000 and 0x00540000/0x004B0000
- `unit_0001.cpp` — 40+ references to 0x004B0000 and 0x00500000
- `unit_0050.cpp` — ~100 references to 0x00240000 (its own code base) and 0x004E0000
- `unit_004E.cpp` — references to 0x00240000
- `unit_004F.cpp` — references to 0x005C0000 and 0x00240000

**Key evidence these are data section bases, not GAME.DAT reads:**
- All accesses use `ld32`/`st32` on `base + offset` patterns — same pattern as stage_group_map (0x00480000 + 24304) and placement table (0x00480000 + 22768)
- No `ps2::disc::read`, `disc::find`, `fopen`, `fread`, or any file I/O calls exist in any generated unit
- The addresses are VA bases for global data tables, accessed via the PS2 memory model

## 4. Changes to disc_folder.cpp or disc.cpp to serve extracted GAME.DAT contents

### Option A: Modify disc_folder.cpp (preferred — no ELF changes)

**Mechanism:** The folder-based disc reader already supports mapping arbitrary byte offsets to individual files via `s_region`. To serve extracted GAME.DAT contents:

1. **Extract GAME.DAT contents:** Use `sdbz::s_archive::extract_all()` (info_archive.cpp:359) to decode INFO.DAT and extract all assets from GAME.DAT to individual files on disk.

2. **Create layout.tsv:** Generate `.ps2-disc/layout.tsv` mapping each extracted file's original GAME.DAT sector LBA to the extracted file path:
   ```
   LBA\tSIZE\tRELATIVE_PATH
   ```

3. **`original_layout()` (disc_folder.cpp:261):** Already supports this — reads layout.tsv, creates `s_region` entries at `lba * kSector` pointing to each extracted file. When the ELF reads a GAME.DAT sector, `folder->read()` maps it to the correct extracted file.

**No changes to disc_folder.cpp needed** — the `original_layout()` code path already exists and handles this exact scenario. Only a layout.tsv file needs to be generated from the INFO.DAT/GAME.DAT archive.

### Option B: Modify disc.cpp (alternative)

Add a new `open()` overload that takes a pre-built `g_index` map (path → {sector, size}) and a custom reader that resolves sectors to extracted files. This would bypass ISO 9660 parsing entirely. More invasive — not recommended unless needed.

## 5. ELF patches needed for SLES_541.61

**Finding: No ELF patches are strictly needed** if using the folder-based approach with `original_layout()`.

**Reasoning:**
- The recompiled ELF (SLES_541.61) accesses GAME.DAT data through the PS2 IOP/CDVD subsystem, which the host runtime intercepts via `ps2::disc::read()`.
- The `ps2::disc::read()` function reads sectors from whatever disc source is mounted (ISO image or folder).
- With `disc_folder.cpp`'s `original_layout()`, extracted GAME.DAT files are mapped to their original LBAs. When the ELF reads sector N, `folder->read()` serves bytes from the correct extracted file.
- The ELF doesn't need to know it's reading from individual files — it just reads sectors.

**When ELF patches WOULD be needed:**
- If the ELF uses raw CDVD IOP commands (sceCdRead, sceCdSeek, etc.) that bypass the `ps2::disc::` API — but the generated units show no such calls; all data access goes through the PS2 memory model.
- If the ELF validates GAME.DAT checksums or signatures — no such validation code was found in generated units.
- If specific sector alignments or sizes are assumed by the ELF that differ from extracted file sizes — but `original_layout()` preserves original LBAs and sizes exactly.

**Conclusion:** Zero ELF patches needed for the folder-based redirect approach. The existing `disc_folder.cpp` infrastructure already supports it.

## 6. Native C++ cross-reference (source/core/)

### `info_archive.cpp` — Archive extraction
- `decode_info()` (line 143): Decodes INFO.DAT using nibble-swap + key-subtraction codec. Key = first 16 bytes. For i >= 16: `decoded[i] = ~swap_nibbles(encoded[i]) - key[i & 15]`.
- `from_decoded_info()` (line 228): Parses 0x30-byte records: 32-byte CP932 name (at +0), u32 unknown (+32), u32 sector (+36), u32 sector_count (+40), u32 byte_size (+44). File count at header offset 0x2C.
- `extract_all()` (line 359): Extracts all assets from GAME.DAT to individual files using `seekg(offset)` + `copy_exact()`.
- `rebuild()` (line 462): Rebuilds GAME.DAT/INFO.DAT with optional replacement files from a directory.

### `binary_reader.cpp` — Primitive readers
- `read_u16_le()`, `read_u32_le()`, `read_i32_le()`, `read_f32_le()` — bounds-checked little-endian readers from `std::span<const std::uint8_t>`.
- `read_file()` / `write_file()` — full file read/write.

### `container.cpp` — Chunked container parsing
- `parse_chunked_container()` — Parses FPIX/FMDL containers with 0x20-byte header (magic, file_size, chunk_count, format), offset table, and chunk data with size validation.

### Comparison: Native vs Recompiled

| Aspect | Native C++ (`source/core/`) | Recompiled generated units |
|--------|----------------------------|---------------------------|
| INFO.DAT decoding | `decode_info()` — nibble swap + key subtraction | Not present (not decompiled) |
| GAME.DAT extraction | `extract_all()` — sector→file mapping | Not present |
| Archive entry lookup | INFO.DAT record walk (0x30-byte stride) | `unit_0030.cpp:L_001C0050` — walks decoded records in memory |
| Direct file I/O | `read_file()`, `write_file()` (binary_reader.cpp) | None — all via PS2 memory model |
| Disc sector reading | Not in native (host-side only) | Via `ps2::disc::read()` (host runtime) |
| Chunked containers | `parse_chunked_container()` | Not present |

## Recommended approach for GAME.DAT redirect

1. **Use `sdbz::s_archive::extract_all()`** (info_archive.cpp:359) to decode INFO.DAT and extract all GAME.DAT assets to individual files on disk.

2. **Generate `.ps2-disc/layout.tsv`** from the decoded INFO.DAT entries:
   ```
   LBA\tBYTE_SIZE\tASSET_NAME
   ```
   (LBA from `sector` field, size from `byte_size` field)

3. **Place extracted files + layout.tsv** in the disc folder alongside the ELF.

4. **Launch with `--disc <folder>`** — `disc_folder.cpp:original_layout()` will map each extracted file to its original LBA. When the ELF reads GAME.DAT sectors, `folder->read()` serves bytes from the correct extracted file.

5. **No ELF patches needed** — the ELF reads sectors through the standard PS2 memory model, which the host runtime translates to `ps2::disc::read()` calls.

No files were modified. Read-only analysis only.
