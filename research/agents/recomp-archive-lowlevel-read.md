# Super Dragon Ball Z Recomp — Low-Level Archive Reading Investigation (Part 2)

## Scope
Deep-dive into low-level archive/container reading in the SDBZ recomp generated units (`C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`), cross-referenced with native C++ (`source/core/binary_reader.cpp`, `info_archive.cpp`, `container.cpp`, `elf.cpp`).

Specific targets:
1. Binary file primitives — read_u32, read_f32, read_string, read_bytes at absolute offsets
2. Program header parsing — PT_LOAD, segment iteration, file_offset → vaddr translation
3. Section header parsing — section iteration, name table, section data
4. Absolute memory reads — ld32 at 0x00480000+ offsets (e.g. placement table)
5. Memory-mapped file access — how archive data is accessed at runtime

## Native C++ reference (for comparison)

### `source\core\binary_reader.cpp`
Bounds-checked little-endian readers over `std::span<const std::uint8_t>`:
- `read_u16_le` (line 15), `read_u32_le` (line 28), `read_i32_le` (line 41), `read_f32_le` (line 52)
- `read_file`/`write_file` (lines 63/97), `fourcc_to_string` (line 116)
- `has_range` bounds-check helper (line 10)

### `source\core\elf.cpp`
- `inspect_elf32` (line 14): validates `\x7fELF` magic, 32-bit LE, version 1
- Reads e_type (0x10), e_machine (0x12), e_entry (0x18), e_phoff (0x1C), e_shoff (0x20),
  e_flags (0x24), e_ehsize (0x28), e_phentsize (0x2A), e_phnum (0x2C), e_shentsize (0x2E), e_shnum (0x30)
- No PT_LOAD/section iteration in inspected portion — header fields only

### `source\core\info_archive.cpp`
- INFO signature `www.arika.co.jp` at offset 1, header `0x30` bytes (lines 23-44)
- `swap_nibbles` (line 28), `validate_info_signature` (line 33), `ascii_fold` (line 46)

### `source\core\container.cpp`
- `parse_chunked_container` (line 16): 32-byte (`0x20`) header
- Fields: magic (0-3), file_size (`read_u32_le @ 4`), chunk_count (`read_u32_le @ 8`),
  format (`read_u32_le @ 12`) — lines 37-39
- Top-level magics: FPIX, FMDL

## Generated recomp units — search results

Searched all `generated\SuperDragonBallZ\*.cpp` for: `read_u32`, `read_f32`, `read_string`,
`read_bytes`, `nibble_swap`, `key_subtraction`, `obfuscate`, `decode_info`, `elf_header`,
`pt_load`, `program_header`, `container_header`, `record_table`, `entry_count`, `offset_table`,
`memory_read`, `PT_LOAD`, `SHT_`, `sh_offset`, `sh_size`, `sh_name`, `p_offset`, `p_vaddr`,
`p_filesz`, `p_memsz`, `memory_map`, `mmap`.

**Result: ZERO matches.** None of these named symbols or ELF/container header field patterns exist.

### What the generated units actually contain

The generated files are flat machine-code translations: each `unit_XXXX.cpp` contains
`void unit_XXXX(Cpu &c, uint32_t entry, uint32_t depth)` with a `switch(entry)` dispatch over
labeled basic blocks (`L_001XXXXX:`). All memory access is inlined as raw `ld32`/`st32`/`ld16`/
`st16`/`ld8`/`st8`/`ld64`/`st64`/`ld128`/`st128` instructions wrapped in `set32(c.r[N], ...)`.
There are no named reader functions, no struct/class definitions, no vtables, and no
archive-specific abstractions.

## 1. Binary file primitives at absolute offsets

No `read_u32`/`read_f32`/`read_string`/`read_bytes` functions exist. The equivalent operations
are inlined `ld32`/`st32` calls. Example from `unit_0000.cpp:613`:
```cpp
set32(c.r[4], ld32((c.r[2].w[0])));   // load 32-bit from address in r[2] into r[4]
set32(c.r[2], ld32((c.r[4].w[0])));   // load 32-bit from address in r[4] into r[2]
st32((c.r[5].w[0]), c.r[2].w[0]);     // store 32-bit r[2] to address in r[5]
```
These operate on register-computed addresses, not absolute immediate offsets. All 240 unit files
contain such operations — they are the generic memory primitives used for ALL data access,
not specifically archive reading.

## 2. Program header parsing (PT_LOAD, segment iteration)

No `PT_LOAD`, `p_offset`, `p_vaddr`, `p_filesz`, `p_memsz` patterns found anywhere. No
program-header iteration or file_offset→vaddr translation logic exists as named/identifiable
code. The native `elf.cpp` `inspect_elf32` reads ELF header fields but the generated units
contain no equivalent ELF parsing.

## 3. Section header parsing (section iteration, name table, section data)

No `SHT_`, `sh_offset`, `sh_size`, `sh_name` patterns found. No section-header iteration,
no section name table lookup, no section data extraction exists as identifiable code.

## 4. Absolute memory reads at 0x00480000+ offsets

**VA range mapping (from table.cpp):**
- `0x00480000`–`0x00483FFF` → `unit_00E0` (table.cpp:223)
- `0x00484000`–`0x00487FFF` → `unit_00E1` (table.cpp:224)
- `0x00488000`–`0x0048BFFF` → `unit_00E2` (table.cpp:225)
- `0x0048C000`–`0x0048FFFF` → `unit_00E3` (table.cpp:226)
- `0x00490000`–`0x00493FFF` → `unit_00E4` (table.cpp:227)
- `0x00494000`–`0x00497FFF` → `unit_00E5` (table.cpp:228)
- `0x0049C000`–`0x0049FFFF` → `unit_00E7` (table.cpp:229)
- `0x004A0000`–`0x004A3FFF` → `unit_00E8` (table.cpp:230)
- `0x004A4000`–`0x004A7FFF` → `unit_00E9` (table.cpp:231)
- `0x004A8000`–`0x004ABFFF` → `unit_00EA` (table.cpp:232)
- `0x004AC000`–`0x004AFFFF` → `unit_00EB` (table.cpp:233)
- `0x004B0000`–`0x004B3FFF` → `unit_00EC` (table.cpp:234)
- `0x004B4000`–`0x004B7FFF` → `unit_00ED` (table.cpp:235)
- `0x004B8000`–`0x004BBFFF` → `unit_00EE` (table.cpp:236)
- `0x004BC000`–`0x004BFFFF` → `unit_00EF` (table.cpp:237)
- `0x004C0000`–`0x004C3FFF` → `unit_00F0` (table.cpp:238)
- `0x004C4000`–`0x004C7FFF` → `unit_00F1` (table.cpp:239)
- `0x004C8000`–`0x004CBFFF` → `unit_00F2` (table.cpp:240)
- `0x004CC000`–`0x004CFFFF` → `unit_00F3` (table.cpp:241)
- `0x004E0000`–`0x004E3FFF` → `unit_00F8` (table.cpp:245)

**Direct absolute ld32 at 0x00480000+ as an immediate: NONE found.** All high-address reads
use register-computed addresses (base + offset), never `ld32(0x0048xxxx)` as an immediate.

**Base address 0x004E0000 / 0x004F0000 loaded into registers** — found in multiple units:

`unit_001C.cpp:2232-2235`:
```cpp
c.r[4].sd[0] = static_cast<std::int32_t>(0x004E0000u);  // r4 = 0x004E0000
c.r[5].sd[0] = static_cast<std::int32_t>(0x004E0000u);  // r5 = 0x004E0000
c.r[6].sd[0] = static_cast<std::int32_t>(0x004F0000u);  // r6 = 0x004F0000
c.r[7].sd[0] = static_cast<std::int32_t>(0x004F0000u);  // r7 = 0x004F0000
set32(c.r[4], c.r[4].w[0] + 12624u);                     // r4 = 0x004E3180
set32(c.r[5], c.r[5].w[0] + 12968u);                     // r5 = 0x004E32A8
set32(c.r[6], c.r[6].w[0] + 29520u);                     // r6 = 0x004F7380
set32(c.r[7], c.r[7].w[0] + 29520u);                     // r7 = 0x004F7380
```
Then at `unit_001C.cpp:2257-2259`:
```cpp
set32(c.r[16], ld32((c.r[21].w[0] - 12u)));   // load from r21-12 (r21 derived from r4=0x004E3180)
set32(c.r[17], ld32((c.r[21].w[0] - 16u)));   // load from r21-16
```
This reads from `0x004E3174` and `0x004E3170` — reads within the 0x004E0000 region, but
without type info cannot be attributed to ELF/container parsing specifically.

`unit_0028.cpp` references `0x004E0000` at 30+ locations (e.g. lines 920, 979, 1097, 1876,
2913, 4290), always loading the base into a register then adding offsets. `unit_0004.cpp`
(lines 1047, 1070), `unit_0027.cpp` (line 864), and `unit_0002.cpp` (line 2048 referencing
0x004B0000) follow the same pattern.

## unit_00F8.cpp — handler for VA 0x004E0000–0x004E3FFF

`unit_00F8.cpp` is the recompiled code unit covering `0x004E0000` (per table.cpp:245). Its
dispatch table (lines 141-360+) maps 220+ entry points to addresses `L_004E0000` through
`L_004E1710+` within that VA range. Example dispatch entries:
- `case 1u: goto L_004E0000;` ... `case 100u: goto L_004E0AAC;` ... `case 220u: goto L_004E1710;`

Its function bodies use `ld32`/`st32` with register-relative offsets, e.g.
`unit_00F8.cpp:1107`:
```cpp
set32(c.r[10], ld32((c.r[3].w[0] - 2720u)));   // load from r3-2720
set32(c.r[7], ld32((c.r[3].w[0] - 2716u)));    // load from r3-2716
```
followed by `st32` stores at stack-relative addresses (`c.r[28].w[0] - 7464u` etc.). These
are stack/working-memory operations, not necessarily archive file reads.

No `ld32(0x004E0000 + N)` immediate absolute reads exist — all high-region access goes
through a base register loaded with `0x004E0000` then offset-added.

## 5. Memory-mapped file access

No `mmap`, `memory_map`, `file_mapping`, or `CreateFileMapping` patterns found. All data
access is through `ld32`/`st32` on register-computed addresses. There is no identifiable
memory-mapped file access layer. The game appears to access archive/container data through
direct memory reads at compiled-in base addresses (like 0x004E0000/0x004F0000) with
runtime-computed offsets — consistent with data already loaded into memory (e.g. ELF
segments or archive data preloaded at fixed addresses), not runtime file mapping.

## Comparison: native vs generated

| Aspect | Native C++ (`source/core/`) | Generated recomp (`generated/`) |
|---|---|---|
| Binary reader | `read_u32_le`/`read_u16_le`/`read_f32_le` with `has_range` bounds checks | Raw `ld32`/`st32` inline, no bounds checks, no named functions |
| INFO.DAT | `swap_nibbles`, `www.arika.co.jp` sig, `0x30` header, record parsing | Not present |
| Container | `parse_chunked_container`, 32-byte header, FPIX/FMDL magic | Not present |
| ELF header | `inspect_elf32`, reads e_entry/e_phoff/e_shoff etc. | Not present |
| PT_LOAD / segment iteration | Not in inspected portion (header fields only) | Not present |
| Section header iteration | Not present | Not present |
| Absolute 0x00480000+ reads | N/A (native reads files via spans) | Base register + offset (e.g. 0x004E0000 + 12624), no immediate |
| Memory-mapped file access | Not present (span-based file reads) | Not present |
| Structs / named fields | `s_elf32_info`, `s_chunked_container` | None — flat functions only |

## Conclusion

No archive/container reading code exists as named functions in the SDBZ recomp generated units.
All low-level memory reads are inlined `ld32`/`st32` instructions across the 240 `unit_*.cpp`
files, operating on register-computed addresses. Base addresses like `0x004E0000`/`0x004F0000`
are loaded into registers then offset-added (e.g. `unit_001C.cpp:2232-2259` reads from
`0x004E3174`/`0x004E3170`). No ELF program/section header parsing, no PT_LOAD iteration, no
memory-mapped file access, and no named binary-reader primitives exist. Recovering
INFO.DAT/GAME.DAT/container/ELF parsing from the recomp would require manual reverse-engineering
of base-address loads and offset arithmetic within the `unit_*.cpp` files.
