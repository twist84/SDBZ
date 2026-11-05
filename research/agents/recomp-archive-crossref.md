# Recomp Archive Cross-Reference — SuperDragonBallZ

Source: `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`
Native code: `C:\Dev\SuperDragonBallZ\source\core\info_archive.cpp`, `container.cpp`, `binary_reader.cpp`
Read-only analysis. No files in ps2-web-recomp were modified.

## Summary

**No INFO.DAT/GAME.DAT archive loading code was found in any of the 240 generated unit files.** The generated units are machine-code recompilations that strip all high-level symbolic names, and no text identifiers related to archive/container loading exist anywhere in the generated code.

The archive loading (INFO.DAT decode, GAME.DAT sector extraction, entry lookup by name, nibble-swap + key-subtraction codec) appears to be either:
1. In unregistered VA gaps (unit_00CA–00CD, unit_00CF–00D1, unit_00E6, unit_00F4 — no generated code exists at these VAs), or
2. Handled by a separate code path not present in the registered 240 units.

## Native Implementation Reference

### `source\core\info_archive.cpp` — 722 lines

| Component | Details |
|---|---|
| Signature | `www.arika.co.jp` (bytes 1–15), byte 0 varies per release (`7F` EU beta, `11` EU, `C2` USA, `60` JPN) |
| Codec | Nibble-swap + inversion + key subtraction: `decoded[i] = (~swap_nibbles(encoded[i]) - key[i & 15]) & 255` |
| Header | 0x30 bytes: 16-byte signature/key, 28 reserved bytes (all zero), u32 file count at 0x2C |
| Record size | 0x30 bytes (48 bytes) per entry |
| Record layout | +0x00: 32-byte NUL-terminated CP932 filename; +0x20: u32 unknown; +0x24: u32 start sector; +0x28: u32 sector count; +0x2C: u32 byte size |
| Sector size | 2048 bytes (0x800) |
| GAME.DAT | No archive-wide header or compression; payloads at `sector_offset * 2048`, read `byte_size` bytes |
| Sector arithmetic | `offset = sector * 2048`, `allocated_size = sector_count * 2048`, `needed_sectors = ceil(byte_size / 2048)` |
| Safety checks | Path traversal prevention, duplicate name detection, overlap detection, extent validation |
| Functions | `decode_info()`, `encode_info()`, `from_encoded_info()`, `from_decoded_info()`, `extract_all()`, `rebuild()` |

### `source\core\container.cpp` — 116 lines

| Component | Details |
|---|---|
| Supported magics | FPIX, FMDL |
| Header | 0x20 bytes: char[4] magic, u32 file_size, u32 chunk_count, u32 format, bytes[16] reserved |
| Offset table | u32[count] at 0x20, absolute chunk offsets |
| Chunk layout | char[4] tag, u32 size (includes header), payload |
| FPIX chunks | XPIX (3,438 found): 32-byte wrapper + TIM2 |
| FMDL tags | DMOD, DMAT, XTEX, XMON, XCUD, XMTX, DHIE, XNON |

### `source\core\binary_reader.cpp` — 120 lines

| Component | Details |
|---|---|
| `read_u16_le` | Little-endian 16-bit read with bounds check |
| `read_u32_le` | Little-endian 32-bit read with bounds check |
| `read_i32_le` / `read_f32_le` | Signed/float variants via `std::bit_cast` |
| `read_file` / `write_file` | Full file read/write |
| `fourcc_to_string` | 4-byte code to string |

## Negative Search Results — Generated Units

| Search Category | Terms | Result |
|---|---|---|
| Archive filenames | `INFO.DAT`, `GAME.DAT`, `info`, `archive`, `container`, `game.dat` | 0 matches |
| Asset extensions | `.stg`, `.mdl`, `.hit`, `.pix`, `.pmt`, `.ase`, `.ams`, `.ani`, `.cht` | 0 matches |
| Codec operations | `swap_nibbles`, `nibble`, `codec`, `decode_info`, `encode_info`, `key_subtraction` | 0 matches |
| Sector arithmetic | `sector`, `sector_size`, `2048` (as sector arithmetic context) | 0 matches as archive-specific |
| Archive structure | `0x30-byte`, `record`, `entry`, `lookup`, `stem`, `companion` | 0 matches |
| Memory addresses | `0x015CE400` (EE INFO.DAT), `0x000BE300` (IOP INFO.DAT), `0x67AC0` (decoder), `0x67800` (ARKD_DVD), `0x72B04` (table ptr) | 0 matches |

## `0x800` / `2048` Matches — Generic Only

The value `2048` (0x800) appears in many units (unit_0000, unit_0001, unit_0002, unit_0004, unit_0005, unit_0006, unit_000D, unit_0012, unit_0013, unit_001C, unit_001D, unit_001E, unit_0023, etc.), but always as a generic constant — stack offsets, buffer sizes, memory strides — **never** as `sector * 2048` GAME.DAT offset arithmetic or `ceil(byte_size / 2048)` sector count calculation. No archive-specific sector arithmetic pattern was identified.

## `0x80000000` Matches — Sign Bit Float

The value `0x80000000` appears widely as the IEEE-754 sign bit for negative floats (−0.0f or sign mask), not as an archive-related constant.

## Comparison Table — Native vs Generated

| Aspect | Native (info_archive.cpp) | Generated Units | Status |
|---|---|---|---|
| INFO.DAT signature validation | `www.arika.co.jp` bytes 1–15 | Not found | ❌ Absent |
| INFO.DAT codec | Nibble-swap + inversion + key subtraction | Not found | ❌ Absent |
| Record count field | u32 at offset 0x2C | Not found | ❌ Absent |
| Record parsing | 0x30-byte records, filename + sector + count + size | Not found | ❌ Absent |
| GAME.DAT sector read | `sector * 2048`, read `byte_size` | Not found | ❌ Absent |
| Sector size constant | 2048 (0x800) | Found generically, not as sector arithmetic | ⚠️ Generic only |
| Entry lookup by name | CP932 filename, ASCII fold, duplicate detection | Not found | ❌ Absent |
| Path safety checks | Traversal prevention, symlink refusal | Not found | ❌ Absent |
| Archive extract/rebuild | `extract_all()`, `rebuild()` with replacement support | Not found | ❌ Absent |
| Container parsing | FPIX/FMDL magic, offset table, chunk extents | Not found | ❌ Absent |
| Little-endian reads | `read_u16_le`, `read_u32_le`, `read_f32_le` | Not found as named functions | ❌ Absent |
| INFO.DAT memory location | EE RAM 0x015CE400, IOP RAM 0x000BE300 | Not referenced | ❌ Absent |
| Decoder routine address | IOP 0x67AC0, EE 0x1C0050 | Not referenced | ❌ Absent |

## Unregistered VA Gaps — Possible Archive Location

The following unit IDs are declared in `units.hpp` but **not registered** in `table.cpp`. No generated code exists at these VAs; archive loading code could reside here:

| Missing Unit IDs | VA Range | Size |
|---|---|---|
| unit_00CA, unit_00CB, unit_00CC, unit_00CD | 0x00428000–0x00437FFF | 64 KB |
| unit_00CF, unit_00D0, unit_00D1 | 0x0043C000–0x00447FFF | 48 KB |
| unit_00E6 | 0x00498000–0x0049BFFF | 16 KB |
| unit_00F4 | 0x004CD000–0x004D3FFF | 32 KB |
| unit_00F9+ | 0x004E4000+ | Unknown (not registered) |

## Recommendations for Archive Cross-Reference

1. **Check unregistered VA gaps:** The 160 KB of unregistered VA space (0x00428000–0x004D3FFF) may contain the archive loading code. These units are declared but not generated — investigate if binary blobs or alternative code sources exist for these ranges.

2. **Compare against EE/IOP RAM snapshots:** The `stage_stall.bin` snapshot referenced in `archive_and_container_notes.md` contains EE RAM at 0x015CE400 and IOP RAM at 0x000BE300 with the decoded INFO.DAT. Cross-reference these memory addresses against generated unit VAs to find the corresponding code.

3. **Trace INFO.DAT loading from game startup:** The archive loading likely occurs during game initialization, before the spawn/fighter lifecycle code. Look for units called at startup that might perform archive setup.

4. **Container format cross-reference:** The native `container.cpp` handles FPIX/FMDL chunked containers. If the recomp reads .stg/.mdl/.hit/.pix companion files, look for container magic validation (`FPIX`, `FMDL`, `XPIX`, `DMOD`, `DMAT`, etc.) as 4-byte immediate values in unit code.

5. **Codec implementation search:** Search for the specific instruction sequence `swap_nibbles` (rotate by 4 bits), `NOT` (inversion), and subtraction with a rotating key — the three operations of the INFO.DAT codec. These would appear as `rol`/`ror` by 4, `not`, and `sub` with a key register.

*Read-only analysis. No files in ps2-web-recomp were modified.*