# Super Dragon Ball Z Recomp — Archive / Low-Level Container Reading Investigation

## Scope
Investigate low-level archive/container reading code in the recompiled SDBZ game (`C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`) and cross-reference with the native C++ implementation in `C:\Dev\SuperDragonBallZ\source\core\` (`binary_reader.cpp`, `info_archive.cpp`, `container.cpp`, `elf.cpp`).

## Native C++ implementation (reference)

### `source\core\binary_reader.cpp`
Type-safe, bounds-checked little-endian readers over a `std::span<const std::uint8_t>`:
- `read_u16_le(data, offset)` — line 15
- `read_u32_le(data, offset)` — line 28
- `read_i32_le(data, offset)` — line 41
- `read_f32_le(data, offset)` — line 52
- `read_file(path)` / `write_file(path, data)` — lines 63/97
- `fourcc_to_string(value)` — line 116
- `has_range(data_size, offset, length)` — line 10 (internal bounds check)

### `source\core\info_archive.cpp` (INFO.DAT)
- INFO signature `www.arika.co.jp` at offset 1, header size `0x30` bytes (lines 23-44)
- `swap_nibbles(value)` nibble swap — line 28
- `validate_info_signature(data)` — line 33
- `ascii_fold(value)` — line 46
- INFO.DAT record parsing across 722 lines

### `source\core\container.cpp` (chunked container: FPIX/FMDL)
- `s_chunked_container::magic_string()` — line 11
- `parse_chunked_container(data, expected_magic)` — line 16
- 32-byte (`0x20`) container header; fields: magic (0-3), file_size (read_u32_le @ 4), chunk_count (read_u32_le @ 8), format (read_u32_le @ 12) — lines 20-47

### `source\core\elf.cpp` (ELF32)
- `inspect_elf32(data)` — line 14
- ELF magic `\x7fELF`, 32-bit, little-endian, version 1 (lines 11-38)
- Reads e_type (0x10), e_machine (0x12), e_entry (0x18), e_phoff (0x1C), e_shoff (0x20), e_flags (0x24), e_ehsize (0x28), e_phentsize (0x2A), e_phnum (0x2C), e_shentsize (0x2E), e_shnum (0x30) via `read_u16_le`/`read_u32_le`

## Generated recomp units — search results

Searched `generated\SuperDragonBallZ\*.cpp` for: `binary_reader`, `read_u32`, `read_f32`, `read_string`, `read_bytes`, `nibble_swap`, `key_subtraction`, `obfuscate`, `decode_info`, `elf_header`, `pt_load`, `program_header`, `container_header`, `record_table`, `entry_count`, `offset_table`, `memory_read`.

**Result: NO matches.** None of these named functions or symbols exist in the generated units.

### What the generated units actually contain

The generated files are flat machine-code translations (`void unit_XXXX(Cpu &c, uint32_t entry, uint32_t depth)`) with a `switch(entry)` dispatch over labeled basic blocks (`L_001XXXXX:`). Low-level memory access is performed via raw `ld32`/`st32`/`ld16`/`st16`/`ld8`/`st8`/`ld64`/`st64`/`ld128`/`st128` instructions wrapped in `set32(c.r[N], ...)` / `st32(..., c.r[N].w[0])` — these are the inlined equivalents of the native `read_u32_le`/`write` primitives. There are no named reader functions, no struct/class definitions, no vtables, and no archive-specific abstractions.

Example from `unit_0000.cpp` (VA range 0x00100000–0x00103FFF):
- Line 613: `set32(c.r[4], ld32((c.r[2].w[0])));` — load 32-bit value from address in r[2] into r[4]
- Line 630: `set32(c.r[2], ld32((c.r[4].w[0])));` — load 32-bit value from address in r[4] into r[2]
- Line 633: `st32((c.r[5].w[0]), c.r[2].w[0]);` — store 32-bit r[2] to address in r[5]
- Line 675: `st32((c.r[29].w[0] + 240u), c.r[2].w[0]);` — stack store at sp+240
- Line 678: `set32(c.r[30], ld32((c.r[2].w[0] - 3200u)));` — load from r[2]-3200

These `ld32`/`st32` operations are scattered across all 240 unit files (e.g. `unit_0000.cpp:613`, `unit_0021.cpp:691`, `unit_0028.cpp:2319`) and serve as the raw memory-read/write primitives for ALL low-level data access — including any INFO.DAT/GAME.DAT/container/ELF parsing that the game performs. They cannot be attributed to a specific archive format without source-level type recovery (debug symbols or manual reverse-engineering of field accesses).

### INFO.DAT-specific code
No `swap_nibbles`, `key_subtraction`, `obfuscate`, or `decode_info` equivalents found. The INFO.DAT signature validation (`www.arika.co.jp`), nibble swap, and record table parsing exist only in the native `info_archive.cpp`, not in the generated recomp units.

### Container/ELF-specific code
No `container_header`, `record_table`, `entry_count`, `offset_table`, `elf_header`, `pt_load`, or `program_header` equivalents found. The native `container.cpp` chunked-container parsing (32-byte header, magic/file_size/chunk_count/format fields) and `elf.cpp` ELF32 inspection exist only in native code, not in the generated units.

## Comparison: native vs generated

| Aspect | Native C++ (`source/core/`) | Generated recomp (`generated/SuperDragonBallZ/`) |
|---|---|---|
| Binary reader | Named `read_u32_le`/`read_u16_le`/`read_f32_le` with bounds checks | Raw `ld32`/`st32`/`ld16`/`st16`/`ld8`/`st8` instructions inline |
| INFO.DAT | `swap_nibbles`, signature `www.arika.co.jp`, `0x30` header, record parsing | Not present as named code |
| Container | `parse_chunked_container`, 32-byte header, FPIX/FMDL magic | Not present as named code |
| ELF | `inspect_elf32`, reads e_entry/e_phoff/e_shoff etc. | Not present as named code |
| Structs/vtables | Named structs (`s_elf32_info`, `s_chunked_container`, `s_camera`) | None — flat functions only |
| Memory safety | `has_range` bounds checks, `std::optional` returns | None — raw register/memory ops |

## Conclusion

No archive/container reading code exists as named functions in the SDBZ recomp generated units. All low-level memory reads are inlined as `ld32`/`st32`-style instructions across the 240 `unit_*.cpp` files. The native `binary_reader.cpp`, `info_archive.cpp`, `container.cpp`, and `elf.cpp` provide typed, bounds-checked, archive-specific abstractions that have no counterpart in the generated code. Recovering INFO.DAT/GAME.DAT/container parsing from the recomp would require manual reverse-engineering of field accesses and offset arithmetic within the `unit_*.cpp` files.
