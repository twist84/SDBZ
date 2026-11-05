# Recompiled VFS/archive loading code — Super Dragon Ball Z

Investigation of how the recompiled game implements VFS/archive browsing UI and entry loading from INFO.DAT/GAME.DAT. Cross-referenced with the native C++ VFS overlay at `C:\Dev\SuperDragonBallZ\source\interface\vfs_overlay.cpp` and `research\archive_and_container_notes.md`. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Source references

- `source\interface\vfs_overlay.cpp` — native C++ VFS overlay implementation (draws archive browser, breadcrumb path, entry list with [dir]/[file] indicators, A/Enter/B/Esc navigation)
- `research\archive_and_container_notes.md` — INFO.DAT/GAME.DAT format documentation, codec details, ELF addresses for ARKD_DVD decoder

## Native C++ VFS overlay (reference)

`vfs_overlay.cpp` (62 lines) implements:
- Draws "ARCHIVE VFS (INFO.DAT/GAME.DAT)" header
- Breadcrumb path from `ctx.vfs_path_components`
- Entry list with [dir]/[file] indicators, selection highlighting
- Navigation help: "A/Enter: open dir / load file | B/Backspace: parent / close | D-pad: navigate | Esc: close"
- Depends on: `ctx.vfs_open`, `ctx.has_font`, `ctx.font`, `ctx.glyph_textures`, `ctx.vfs_archive`, `ctx.vfs_entries`, `ctx.vfs_selected`, `ctx.vfs_path_components`

## INFO.DAT/GAME.DAT format (from research notes)

- INFO.DAT: 98,304 bytes, 0x30-byte header, 1,934 records of 0x30 bytes, then padding
- Codec: nibble swap + inversion + key subtraction (reversible obfuscation)
- Signature: `\x7fwww.arika.co.jp`
- Each record: 32-byte CP932 filename, u32 unknown, u32 start sector, u32 sector count, u32 byte size
- GAME.DAT: payloads at `sector_offset * 2048`, no compression
- ELF addresses: ARKD_DVD at IOP 0x67800, decoder at 0x67AC0, nibble swaps at 0x67B88/0x67B8C, inversion at 0x67BBC, key subtraction at 0x67BF4, table pointer at 0x72B04

## Search results — generated units

No high-level VFS/archive string literals exist in any generated unit. This is expected because these are low-level MIPS instruction emulator units — VFS/archive code is implemented as register operations and memory accesses without high-level keyword strings.

| Keyword(s) searched | Matches | Notes |
|---|---|---|
| vfs_overlay, archive_browser, vfs_open, breadcrumb | 0 | — |
| load_archive_model, load_texture, load_font, load_motion | 0 | — |
| .mdl, .pix, .fon, .pmt, .mot, .stg, .hit | 0 | — |
| parent_dir, open_dir, close_vfs | 0 | — |
| INFO.DAT, GAME.DAT, info.dat, game.dat | 0 | — |
| archive, ARCHIVE, sector, SECTOR | 0 | — |
| open_file, OPEN_FILE, read_file, READ_FILE | 0 | — |
| file_type, FILE_TYPE, extension, EXTENSION, loadable, LOADABLE | 0 | — |
| arkd, ARKD, arkd_dvd, ARKD_DVD, 0x67, 0x015C, 0x72B0, 0x1C00 | 2 matches (unit_008B.cpp only) |

## Potential codec-related constants — unit_008B.cpp

Two constants in the 0x67xx0000 range were found, which may be related to the INFO.DAT codec or ARKD_DVD module:

### unit_008B.cpp line 2424
```cpp
c.r[21].sd[0] = static_cast<std::int32_t>(0x676F0000u);
```
Context (lines 2410-2439): Part of a sequence of 128-bit vector stores to stack frame (c.r[29] + 752 through 816), with arithmetic operations combining registers. The value `0x676F0000` is loaded into r21 then combined with other values via OR and shifts. This pattern is consistent with codec key material or decoding constants being set up.

### unit_008B.cpp line 3530
```cpp
c.r[3].sd[0] = static_cast<std::int32_t>(0x67450000u);
```
Context (lines 3525-3556): Function at L_0032EC50 (VA ~0x0032EC50) sets up a stack frame (subtracts 128 from r29), stores r31, then loads `0x67450000` into r3, ORs it with 8961 (0x2301), stores to stack at c.r[29] + 32. Additional constants `0xEFCD0000`, `0x98BA0000`, `0x10320000` are loaded and combined. This looks like it could be building the INFO.DAT codec key or performing nibble-swap/inversion operations matching the codec described in the research notes.

The sequence `0x67450000 | 8961`, `0xEFCD0000 | 43913`, `0x98BA0000 | 56574`, `0x10320000 | 21622` resembles the construction of 32-bit values from 16-bit halves — consistent with building codec lookup tables or key values for the INFO.DAT obfuscation.

## Comparison: native C++ vs recompiled

| Aspect | Native C++ (vfs_overlay.cpp) | Recompiled generated units | Status |
|---|---|---|---|
| VFS overlay UI | draw_vfs_overlay() draws header, breadcrumb, entry list, help text | No equivalent found — no UI string literals | ❌ Not present |
| Breadcrumb path | Built from `ctx.vfs_path_components[]` | Not found | ❌ Not present |
| Entry selection | `ctx.vfs_selected` with scrolling | Not found | ❌ Not present |
| File open/load | A/Enter to open dir/load file | Not found | ❌ Not present |
| Parent dir/close | B/Backspace/Esc | Not found | ❌ Not present |
| INFO.DAT codec | Not in native C++ (Python tools only) | Possible codec constants in unit_008B.cpp (0x676F0000, 0x67450000) | ⚠️ Possible match |
| GAME.DAT sector loading | Not in native C++ (Python tools only) | Not found | ❌ Not present |
| Archive entry table | 1,934 records of 0x30 bytes | Not found | ❌ Not present |
| File extensions (.mdl, .pix, etc.) | Not in native C++ | Not found | ❌ Not present |

## ELF addresses vs generated unit VAs

The research notes identify ELF addresses:
- ARKD_DVD module at IOP 0x67800
- Decoder routine at 0x67AC0
- Nibble swaps at 0x67B88/0x67B8C
- Inversion at 0x67BBC
- Key subtraction at 0x67BF4
- Table pointer at 0x72B04
- EE lookup at 0x1C0050

These ELF addresses do not directly appear in the generated units. The generated units use their own VA ranges (e.g., unit_008B.cpp contains VAs in the 0x0032xxxx–0x0042xxxx range). The `0x676F0000` and `0x67450000` constants found in unit_008B.cpp are in a similar magnitude to the IOP 0x67xxxx range but are used as immediate arithmetic constants, not as direct address references.

## Conclusion

**No VFS/archive browsing UI code is present in the generated units.** The decompiled units are low-level MIPS instruction emulators that do not contain:
- High-level VFS function names ("vfs_open", "archive_browser", etc.)
- File extension strings (".mdl", ".pix", ".fon", etc.)
- Archive filenames ("INFO.DAT", "GAME.DAT")
- UI navigation text ("parent_dir", "open_dir", "close_vfs")
- Any UI rendering code for the archive browser

**Two potential codec-related constants** were found in `unit_008B.cpp` (`0x676F0000u` at line 2424, `0x67450000u` at line 3530) that may relate to the INFO.DAT obfuscation codec described in the research notes, but this is speculative — the values are used as arithmetic constants, not as direct string or address references.

To find VFS/archive loading code in the recompiled game, one would need to:
1. Map ELF addresses (0x67800–0x67BF4 range for ARKD_DVD decoder) to generated unit VAs
2. Trace INFO.DAT/GAME.DAT sector offset calculations through register operations
3. Identify the codec decode loop (nibble swap, inversion, key subtraction) at the instruction level

This requires cross-referencing with the original ELF binary (SLES_541.61) rather than keyword search in the generated units.
