# Recomp GAME.DAT Loading Code Search Results

## Search scope

Searched ALL generated `.cpp` files under `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\` for GAME.DAT archive container loading code.

Files searched:
- `unit_0000.cpp` through `unit_00F8.cpp` (EE MIPS decompiled units)
- `vu1_000.cpp` through `vu1_008.cpp` + `vu1_table.cpp` (VU1 microcode units)
- `table.cpp` (unit dispatch table)

Total: ~130 `.cpp` files

## Search terms used

| Category | Terms searched | Matches found |
|----------|---------------|---------------|
| GAME.DAT container | gamedat, game_dat, container_read, sector_read | **None** |
| Archive entry extraction | extract_entry, read_entry, file_offset, file_size | **None** |
| Companion file loading | load_companion, load_mdl, load_hit | **None** |
| Archive VFS browsing | vfs_overlay, archive_browse, breadcrumb | **None** |
| Disc/file I/O | dvd, cdvd, sceCd, sceIo, file_handle, file_read | **None** |
| Sector size | 0x2000, 2048 (as sector context) | Found as general memory alignment constants only |

## Key finding: No GAME.DAT loading code found

**No GAME.DAT archive container loading code exists in any generated unit.** The decompiled MIPS code does not contain:

1. **GAME.DAT container reading** — No code that opens, seeks, or reads from GAME.DAT
2. **Archive entry extraction** — No `extract_entry`, `read_entry`, `file_offset`, or `file_size` logic
3. **Companion file loading** — No `load_mdl`, `load_hit`, `load_companion`, or companion file loading from archive
4. **Archive VFS browsing** — No VFS overlay, archive browsing, or breadcrumb logic
5. **Sector-based reading** — No `sceCd`, `sceIo`, `dvd_read`, or `file_read` calls that would read sectors from GAME.DAT

The `2048` (sector size) constant appears in many units (unit_0000, unit_0001, unit_0002, unit_0004, unit_0007, unit_0008, unit_000A, unit_000C, unit_000D, unit_0012, unit_0013, unit_0014, unit_0015, unit_0016) but only as **general memory alignment constants** (stack alignment, buffer sizing), not as GAME.DAT sector reads.

## VA base addresses found (not GAME.DAT-specific)

The following VA base addresses appear in multiple units for data table lookups, but they are NOT GAME.DAT archive access:

| VA base | Units | Purpose |
|---------|-------|---------|
| 0x00480000 | unit_004E, unit_004C, unit_004A, unit_004F, unit_0050, many others | Stage_group_map, placement table, global constants |
| 0x005C0000 | unit_0050, unit_004F, unit_0026, unit_0027, unit_002A, unit_0028, unit_002E, unit_002C, unit_002B, unit_002F, unit_0030, unit_0031, unit_003D | Data section base for table lookups |
| 0x005E0000 | unit_0004, unit_0026, unit_0027, unit_0028, unit_002A, unit_002E, unit_002C, unit_002B, unit_002F, unit_0030, unit_0031, unit_003D | Data section base for table lookups |
| 0x00550000 | unit_0005 | Data section base for table lookups |

These VA bases are used for reading global data tables from the ELF binary (similar to how stage_group_map and placement table are read at 0x00480000 + offset), not for GAME.DAT archive access.

## unit_0050.cpp (VA 0x00240000-0x00243FFF)

This unit has ~400 dispatch entries and is referenced by multiple other units. It contains general utility code (data table lookups, memory operations, calls to units 0027, 004E) but **no GAME.DAT-specific loading code**. The VA base 0x00240000 is its own code section, not an archive base.

## Comparison with native C++ implementation

**Native C++ (`source/core/container.cpp`):**
- `parse_chunked_container()` — parses FPIX/FMDL chunked containers with header, chunk count, offset table, and chunk data
- Reads magic, file_size, chunk_count, format from 0x20-byte header
- Validates chunk offsets, sizes, and extents
- Returns parsed chunk list with offsets and sizes

**Native C++ (`research/archive_and_container_notes.md`):**
- INFO.DAT: 98,304 bytes, 0x30-byte header, 1,934 records of 0x30 bytes each
- Each record: 32-byte NUL-terminated CP932 filename, u32 unknown, u32 starting sector, u32 sector count, u32 actual file size
- GAME.DAT: No archive-wide header or compression; payloads at `sector_offset * 2048`, read `byte_size` bytes
- Sector-based reading with 2048-byte sectors

**Generated recompiled code:**
- No equivalent GAME.DAT loading code found
- No sector-based reading logic
- No INFO.DAT decoding logic
- No FPIX/FMDL container parsing
- File/asset loading from GAME.DAT appears to be **not yet decompiled/recompiled** into C++ units

## Conclusion

GAME.DAT archive container loading functionality is **not present in the generated recompiled code**. It may be:

1. Entirely in the original ELF binary and not yet decompiled into C++ units
2. Handled through raw PS2 DVD/CDVD IOP calls not visible in the EE decompiled code
3. Implemented through memory-mapped access without identifiable keywords

No files were modified. Read-only analysis only.
