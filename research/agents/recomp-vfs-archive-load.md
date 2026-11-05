# Recompiled VFS/archive loading code — Super Dragon Ball Z

Investigation of how the recompiled game implements VFS/archive browsing and entry loading from INFO.DAT/GAME.DAT, cross-referenced with native C++ implementations. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Source references

- `source\interface\vfs_overlay.cpp` — native C++ archive VFS overlay (draws browser UI, breadcrumb path, entry list with [dir]/[file] indicators)
- `source\interface\debug_menu\debug_menu.cpp` — native C++ debug menu (includes "Archive VFS: loaded/not loaded" item, VFS open/close toggle)
- `research\archive_and_container_notes.md` — INFO.DAT/GAME.DAT format documentation, ELF addresses for ARKD_DVD decoder

## Native C++ implementations (reference)

### vfs_overlay.cpp (62 lines)

Draws the archive VFS browser overlay:
- Header: "ARCHIVE VFS (INFO.DAT/GAME.DAT)"
- Breadcrumb path from `ctx.vfs_path_components`
- Entry list with `[dir]` / `[file]` indicators and selection highlighting
- Navigation help: "A/Enter: open dir / load file | B/Backspace: parent / close | D-pad: navigate | Esc: close"
- Depends on: `ctx.vfs_open`, `ctx.vfs_archive`, `ctx.vfs_entries`, `ctx.vfs_selected`, `ctx.vfs_path_components`

### debug_menu.cpp (329 lines)

`debug_menu_build()` builds menu items including:
- "Stage", "Motion" entries
- "Show effect", "Show collision", "Show templates" toggles
- "Archive VFS: loaded / not loaded" item
- "Fit camera", "Quit" items

`debug_menu_input()` handles navigation and toggles `vfs_open`, `vfs_path_components`, `vfs_selected`.

## INFO.DAT/GAME.DAT format (from research notes)

- INFO.DAT: 98,304 bytes, 0x30-byte header, 1,934 records of 0x30 bytes, then padding
- Codec: nibble swap + inversion + key subtraction (reversible obfuscation)
- Signature: `\x7fwww.arika.co.jp`
- Each record: 32-byte CP932 filename, u32 unknown, u32 start sector, u32 sector count, u32 byte size
- GAME.DAT: payloads at `sector_offset * 2048`, no compression, 338,550 sectors / 693,350,400 bytes indexed region
- ELF addresses: ARKD_DVD at IOP 0x67800, decoder at 0x67AC0, nibble swaps at 0x67B88/0x67B8C, inversion at 0x67BBC, key subtraction at 0x67BF4, table pointer at 0x72B04

## Search results — generated units

No high-level VFS/archive string literals exist in any generated unit. These are low-level MIPS instruction emulator units — VFS/archive code is implemented as register operations and memory accesses without high-level keyword strings.

| Keyword(s) searched | Matches | Notes |
|---|---|---|
| open_archive, close_archive, archive_handle | 0 | — |
| list_entries, enumerate, entry_count | 0 | — |
| find_entry, lookup_entry, entry_by_name | 0 | — |
| load_companion, load_stage_assets, load_mdl_hit_pix | 0 | — |
| breadcrumb, parent_dir, current_path | 0 | — |
| vfs_overlay, archive_browser, vfs_open | 0 | (from prior search) |
| .mdl, .pix, .fon, .pmt, .mot, .stg, .hit | 0 | (from prior search) |
| INFO.DAT, GAME.DAT | 0 | (from prior search) |

## Comparison: native C++ vs recompiled

| Aspect | Native C++ | Recompiled generated units | Status |
|---|---|---|---|
| VFS overlay UI | draw_vfs_overlay() draws header, breadcrumb, entry list | No equivalent found — no UI string literals | ❌ Not present |
| Debug menu VFS item | "Archive VFS: loaded/not loaded" in debug_menu_build() | Not found | ❌ Not present |
| VFS open/close | debug_menu_input() toggles vfs_open | Not found | ❌ Not present |
| Breadcrumb navigation | Built from vfs_path_components[] | Not found | ❌ Not present |
| Entry enumeration | 1,934 INFO.DAT records | Not found | ❌ Not present |
| File loading by sector | GAME.DAT at sector_offset * 2048 | Not found | ❌ Not present |
| Companion assets (.stg+.mdl+.hit+.pix) | Not explicitly in native C++ | Not found | ❌ Not present |
| INFO.DAT codec | Python tools only | Possible constants in unit_008B.cpp (see below) | ⚠️ Speculative |

## Speculative codec constants — unit_008B.cpp

Two constants in the 0x67xx0000 range were found (from prior analysis in recomp-vfs-loading.md):

- **Line 2424**: `c.r[21].sd[0] = static_cast<std::int32_t>(0x676F0000u);` — vector setup for stack frame stores at c.r[29]+752 through 816
- **Line 3530**: `c.r[3].sd[0] = static_cast<std::int32_t>(0x67450000u);` — in function L_0032EC50, combined with OR/shift operations to build 32-bit values

These constants are in a similar magnitude to the IOP 0x67xxxx ARKD_DVD address range mentioned in the research notes, but they are used as arithmetic constants (ORed with values like 8961, 43913, 56574, 21622), not as direct address references or string literals.

## ELF address mapping

The research notes identify ELF addresses:
- ARKD_DVD module at IOP 0x67800
- Decoder routine at 0x67AC0
- Nibble swaps at 0x67B88/0x67B8C
- Inversion at 0x67BBC
- Key subtraction at 0x67BF4
- Table pointer at 0x72B04

None of these ELF addresses appear directly in the generated units. The generated units use their own VA ranges (e.g., unit_008B.cpp contains VAs in the 0x0032xxxx–0x0042xxxx range). Direct VA-to-ELF-address mapping would require analysis of the original ELF binary (SLES_541.61).

## Conclusion

**No VFS/archive browsing or loading code is identifiable** in the generated units at the string-literal level. The decompiled units are low-level MIPS instruction emulators that do not contain:
- High-level VFS function names ("open_archive", "close_archive", "archive_handle", etc.)
- Entry enumeration names ("list_entries", "enumerate", "entry_count")
- Entry lookup names ("find_entry", "lookup_entry", "entry_by_name")
- Companion loading names ("load_companion", "load_stage_assets", "load_mdl_hit_pix")
- VFS path navigation names ("breadcrumb", "parent_dir", "current_path")
- Archive filenames ("INFO.DAT", "GAME.DAT")
- File extension strings (".mdl", ".pix", ".fon", ".pmt", ".mot", ".stg", ".hit")
- Any UI rendering code for the archive browser or debug menu VFS item

**To find VFS/archive loading code**, one would need to:
1. Map ELF addresses (0x67800–0x67BF4 ARKD_DVD decoder range) to generated unit VAs
2. Trace INFO.DAT/GAME.DAT sector offset calculations through register operations
3. Identify the codec decode loop (nibble swap, inversion, key subtraction) at the instruction level

This requires cross-referencing with the original ELF binary (SLES_541.61) rather than keyword search in the generated units.
