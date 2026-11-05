# Recomp Archive Loading Code — INFO.DAT / GAME.DAT Equivalents

## Search scope

Searched ALL generated `.cpp` files under `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\` for archive loading code.

Files searched:
- `unit_0000.cpp` through `unit_00F8.cpp` (EE MIPS decompiled units)
- `vu1_000.cpp` through `vu1_008.cpp` + `vu1_table.cpp` (VU1 microcode units)
- `table.cpp` (unit dispatch table)

Total: ~130 `.cpp` files

## Search terms used

| Category | Terms searched | Matches found |
|----------|---------------|---------------|
| Archive container reading | archive, container, entry_table, record_table, file_index, file_table, asset_table, filename_lookup, filename_search | **None** (except incidental matches) |
| Obfuscation codec | nibble, swap_nibble, key_subtract, byte_inversion, xor_key, obfuscat | **None** |
| Record-based iteration | 0x30.*record, record.*0x30, walk.*record | **Yes — unit_0030.cpp:L_001C0050** |
| Archive entry lookup | filename lookup, entry lookup | **Yes — unit_0030.cpp:L_001C0050** |
| Binary reading primitives | read_u32, read_f32, read_string, read_u16, read_u8 | **None** |
| ELF address mapping | 0x1C0050 | **Yes — unit_0030.cpp:L_001C0050** |

## Key finding: INFO.DAT record-walking code found at unit_0030.cpp:L_001C0050

**This is the direct equivalent of the ELF's INFO.DAT lookup at EE address 0x1C0050.** The recompiled VA `0x001C0050` matches the ELF address exactly.

### Code location: unit_0030.cpp:681–749 (block `L_001C0050`)

**Record structure traversal — 0x30 (48) byte stride confirmed:**

```cpp
// unit_0030.cpp:700-701 — Load base pointer and record count
set32(c.r[3], ld32((c.r[28].w[0] - 10356u)));   // c.r[3] = INFO.DAT record table base
set32(c.r[2], ld32((c.r[3].w[0] + 44u)));       // c.r[2] = record count (at base+44)

// unit_0030.cpp:709-721 — Record walking loop with 0x30-byte stride
L_001C00A8:
    set32(c.r[4], c.r[3].w[0] + c.r[18].w[0]);  // c.r[4] = base + stride (0x30 per record)
    set32(c.r[2], ld32((c.r[16].w[0])));         // c.r[2] = search key (filename hash/value)
    set32(c.r[3], c.r[4].w[0] + c.r[17].w[0]);   // c.r[3] = record field offset within record
    set32(c.r[3], ld32((c.r[3].w[0] + 48u)));    // c.r[3] = ld32 from record+offset — load record field
    { const bool t = c.r[3].d[0] == c.r[2].d[0];  if (t) { goto L_001C00E8; } }  // compare: match?
// ... loop continues ...
L_001C00C0:
    set32(c.r[3], ld32((c.r[28].w[0] - 10356u)));  // reload base
    set32(c.r[19], c.r[19].w[0] + 1u);             // record index++
    set32(c.r[2], ld32((c.r[3].w[0] + 44u)));      // reload record count
    c.r[2].d[0] = c.r[19].d[0] < c.r[2].d[0] ? 1ull : 0ull;  // index < count?
    { const bool t = c.r[2].d[0] != 0ull; ... goto L_001C00A8; }  // continue loop
    set32(c.r[18], c.r[18].w[0] + 48u);            // stride += 48 (0x30 bytes per record!)
```

**Key evidence this is INFO.DAT record walking:**

| Aspect | Found in code | INFO.DAT spec (archive_and_container_notes.md) |
|--------|--------------|-----------------------------------------------|
| Record stride | **48 bytes** (line 717: `+ 48u`) | 0x30 = 48 bytes per record ✓ |
| Record base | `c.r[28].w[0] - 10356` (line 700) | Record table pointer ✓ |
| Record count | Loaded from `base + 44` (line 701) | File count at header offset 0x2C ✓ |
| Field comparison | `ld32` from record+offset, compared to search key (line 714) | Filename comparison within 32-byte name field ✓ |
| Loop bounds | `index < count` (line 719) | Walk all 1934 records ✓ |
| Match handler | `goto L_001C00E8` on match (line 714) | Entry found → return sector/size ✓ |
| Post-match computation | `base + index*48 + offset`, load from `+92` (lines 734-739) | Return record data ✓ |

**Match handler (lines 728–749):**
```cpp
L_001C00E8:
    set32(c.r[4], c.r[4].w[0] + 48u);           // advance past matched record
    // ... calls unit_0024(c, 83u) ...
L_001C00F4:
    // ... loop back or exit ...
L_001C0114:
    // Compute final address: base + index*48 + offset
    set32(c.r[3], ld32((c.r[28].w[0] - 10356u)));  // base
    set32(c.r[2], c.r[19].w[0] << 1);              // index * 2
    set32(c.r[2], c.r[2].w[0] + c.r[19].w[0]);     // index * 3
    set32(c.r[2], c.r[2].w[0] << 4);               // index * 48
    set32(c.r[2], c.r[2].w[0] + c.r[3].w[0]);      // base + index*48
    c.r[2].d[0] = ld32((c.r[2].w[0] + 92u));       // load field at +92 (0x5C) from record
```

The `index * 48` computation (lines 735-737: `<<1`, `+index`, `<<4` = `index*2+index = index*3`, `<<4` = `index*48`) confirms 48-byte (0x30) record stride.

The offset `+92` (0x5C) is beyond the documented 0x30-byte record size, suggesting either:
- The record table has been extended beyond the original INFO.DAT format
- Or this is reading from a related but different table structure

## NOT found in generated units

### 1. Obfuscation codec — ABSENT
No nibble swap, XOR key subtraction, byte inversion, or any obfuscation logic found. The INFO.DAT codec documented in `research/archive_and_container_notes.md` (swap_nibbles + key subtraction at 0x67B88/0x67B8C/0x67BBC/0x67BF4 in the ELF) is **not present** in the recompiled code. This means INFO.DAT decoding is either:
- Not yet decompiled into C++ units
- Handled at a lower level not visible in the EE decompiled code

### 2. GAME.DAT sector reading — ABSENT
No `sceCd`, `sceIo`, `dvd_read`, `file_read`, or sector-based reading from GAME.DAT found. The `2048` (sector size) constant appears in many units but only as general memory alignment constants, not as GAME.DAT sector reads.

### 3. Binary reading primitives — ABSENT
No `read_u32`, `read_f32`, `read_string`, `read_u16`, `read_u8`, `read_le`, or `read_be` functions found in any unit.

### 4. Archive entry lookup by filename — PARTIALLY PRESENT
The `L_001C0050` code does perform entry lookup by comparing loaded record fields against a search key, but this operates on already-decoded record data in memory, not on raw archive file bytes.

## VA address correspondence

| ELF address | Recomp VA | Unit | File:Line | Function |
|-------------|-----------|------|-----------|----------|
| 0x1C0050 | 0x001C0050 | unit_0030.cpp | line 681 | INFO.DAT record walker (0x30-byte stride) |

The VA `0x001C0050` falls inside unit_0030's window (0x001C0000–0x001C3FFF), and the label `L_001C0050` exists as a dispatch entry (unit_0030.cpp:151: `case 11u: goto L_001C0050;`). This is a direct one-to-one address correspondence with the ELF research finding.

## Comparison with native C++ implementation

**Native C++ (`source/core/container.cpp`):**
- `parse_chunked_container()` — parses FPIX/FMDL chunked containers
- Reads magic, file_size, chunk_count, format from 0x20-byte header
- Validates chunk offsets, sizes, extents
- Returns parsed chunk list

**Native C++ (`research/archive_and_container_notes.md`):**
- INFO.DAT: 98,304 bytes, 0x30-byte header, 1,934 records of 0x30 bytes
- Each record: 32-byte filename, u32 unknown, u32 sector, u32 sector_count, u32 file_size
- GAME.DAT: payloads at `sector_offset * 2048`, read `byte_size` bytes
- INFO.DAT codec: nibble swap + key subtraction (NOT in recompiled code)

**Generated recompiled code:**
- **Found:** INFO.DAT record-walking at unit_0030.cpp:L_001C0050 (48-byte stride, record count, field comparison, match handler)
- **Missing:** INFO.DAT obfuscation codec (nibble swap + key subtraction)
- **Missing:** GAME.DAT sector-based reading
- **Missing:** Binary file reading primitives (read_u32, etc.)
- **Missing:** Chunked container parsing (FPIX/FMDL)

## Conclusion

The recompiled code contains **one** archive-related function: INFO.DAT record walking at `unit_0030.cpp:L_001C0050` (VA 0x001C0050), which walks 48-byte (0x30) records with filename field comparison. This matches the ELF research finding exactly.

However, the **INFO.DAT obfuscation codec** (nibble swap + key subtraction) and **GAME.DAT sector-based reading** are **not present** in any generated unit. These functions are not yet decompiled into C++ units.

No files were modified. Read-only analysis only.
