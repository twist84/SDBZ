# Recompiled Code — INFO.DAT / GAME.DAT Archive Loading Search Results

## 1. Summary

**No INFO.DAT or GAME.DAT archive loading code exists in any of the 240 generated EE units** (`unit_0000.cpp` through `unit_00F8.cpp`) or the 9 VU1 microcode files (`vu1_000.cpp` through `vu1_008.cpp`) in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`.

This is expected based on the PS2 architecture and the ELF research: the INFO.DAT/GAME.DAT archive codec (nibble-swap + bit inversion + key subtraction) runs on the **IOP (Input/Output Processor)**, not the EE (Emotion Engine). The EE code that was decompiled only receives already-decoded data via IPC/SIF or shared memory. The ps2-web-recomp project decompiles EE code only, so IOP-side archive loading is outside its scope.

## 2. Native C++ Implementation (for comparison)

Our native implementation at `C:\Dev\SuperDragonBallZ\source\core\info_archive.cpp` implements:

| Feature | Native Implementation |
|---------|----------------------|
| INFO.DAT signature | `www.arika.co.jp` at bytes 1–15 (byte 0 varies by release) |
| Codec key | First 16 bytes of INFO.DAT |
| Decode (per byte i >= 16) | `decoded[i] = (~swap_nibbles(encoded[i]) - key[i & 15]) & 255` |
| Encode | `encoded[i] = swap_nibbles(~(decoded[i] + key[i & 15]) & 255)` |
| Record size | 0x30 (48) bytes |
| Record fields | +0x00: 32-byte CP932 filename, +0x20: unknown u32, +0x24: start sector, +0x28: sector count, +0x2C: byte size |
| File count | u32 at decoded +0x2C |
| GAME.DAT read | `sector_offset * 2048`, read `byte_size` bytes |
| INFO.DAT size | 98,304 bytes (0x18000) |
| GAME.DAT size | 730,050,560 bytes |

The native container code (`container.cpp`) handles FPIX/FMDL chunked containers within extracted assets, not the outer INFO.DAT/GAME.DAT archives.

## 3. ELF Research (for comparison)

From `research/archive_and_container_notes.md`:

| Detail | Value |
|--------|-------|
| IOP decoder module | ARKD_DVD at IOP 0x67800 |
| Decoder routine | IOP 0x67AC0 (module RVA 0x2C0) |
| Nibble swap | IOP 0x67B88/0x67B8C |
| Bit inversion | IOP 0x67BBC |
| Key subtraction | IOP 0x67BF4 |
| Table pointer | IOP 0x72B04 |
| EE lookup | ELF 0x1C0050 walks 0x30-byte records |
| Decoded buffer location | EE RAM 0x015CE400, IOP RAM 0x000BE300 |

All IOP addresses (0x67800–0x72B04 range) are outside the decompiled EE VA range (`0x00100000`–`0x004E0000`). The EE lookup at ELF 0x1C0050 does not correspond to any recompiled VA containing archive code.

## 4. Search Results by Category

### 4.1 INFO.DAT Parsing — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\binfo\b` | No matches |
| `\binfodat\b` | No matches |
| `\binfo_dat\b` | No matches |
| `"INFO.DAT"` | No matches |
| `\bdecode\b` | No matches |
| `\bencode\b` | No matches |
| `\bnibble\b` | No matches |
| `\bswap_nibble\b` | No matches |
| `\barkd\b`, `\bARKD\b` | No matches |
| `0x7F777777` (signature start) | No matches |
| `arika` | No matches |

### 4.2 GAME.DAT Parsing — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bgamedat\b` | No matches |
| `\bgame_dat\b` | No matches |
| `"GAME.DAT"` | No matches |
| `\bdvd\b`, `\bCD\b`, `\bdisc\b` | No matches |
| `\bsceIo\b`, `\bsceCd\b` | No matches |
| `\bopen\b`, `\bread\b`, `\bseek\b`, `\bclose\b` | No matches |

### 4.3 VFS / Archive Browsing — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bvfs\b` | No matches |
| `\barchive\b` | No matches |
| `\bcontainer\b` | No matches |
| `\blookup\b` | No matches |
| `\bentry\b` (whole word) | Only dispatch function parameter names |
| `\brecord\b` (whole word) | No matches |

### 4.4 Container Format — NOT FOUND (in generated units)

| Search term | Result |
|-------------|--------|
| `\barchive_header\b` | No matches |
| `\brecord_count\b` | No matches |
| `\bsector_size\b` | No matches |
| `2048` | Matches exist but are buffer/memory sizes, not sector size (e.g., `set32(c.r[6], 0u + 2048u)`) |

### 4.5 Nibble-Swap Codec — NOT FOUND

| Codec operation | Result |
|-----------------|--------|
| Nibble swap `(x & 15) << 4 \| x >> 4` | No matches |
| Bit inversion `~x` on byte values | No matches (outside FPU context) |
| Key subtraction `x - key[i & 15]` | No matches |

## 5. Files Searched

All 249 files in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`:
- 240 EE units: `unit_0000.cpp` through `unit_00F8.cpp` (VA `0x00100000`–`0x004E0000`)
- 9 VU1 microcode units: `vu1_000.cpp` through `vu1_008.cpp`
- `table.cpp` (unit address mapping)
- `units.hpp` (unit function declarations)

## 6. Conclusion

The ps2-web-recomp EE decompilation contains **zero** INFO.DAT/GAME.DAT archive loading code. The archive system operates entirely on the IOP:

1. **IOP side** (not decompiled): ARKD_DVD module loads INFO.DAT, applies nibble-swap + inversion + key-subtraction codec, parses the 1,934-record table, and serves file data from GAME.DAT at `sector * 2048`.

2. **EE side** (decompiled): Receives decoded asset data via IPC/SIF or shared memory, with no direct archive file I/O.

To investigate archive loading further, IOP-side code would need to be decompiled separately, or SIF command tracing from EE to IOP would need to be performed.

## 7. Comparison: Native vs. Generated

| Aspect | Native C++ (`info_archive.cpp`) | Generated EE Units |
|--------|--------------------------------|---------------------|
| INFO.DAT decode | Implemented (nibble swap + invert + key sub) | Not present |
| INFO.DAT record parsing | 48-byte records, 5 fields each | Not present |
| GAME.DAT sector reading | `sector * 2048`, read `byte_size` | Not present |
| Archive entry lookup | By name, case-folded, CP932 | Not present |
| FPIX/FMDL containers | `container.cpp` parses chunked containers | Not present |
| Disc/DVD access | `std::ifstream` on host filesystem | Not present (no `sceIo`/`sceCd`) |
