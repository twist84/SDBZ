# Recomp Direct Loading Investigation — SuperDragonBallZ

Source: `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`
Native code: `C:\Dev\SuperDragonBallZ\source\` (sdl_main.cpp, vfs_overlay.cpp, debug_menu.cpp, info_archive.cpp, etc.)
Read-only analysis. No files in ps2-web-recomp were modified.

## Summary

**No direct file loading, bypass, or debug mode code was found in any of the 240 generated unit files.** The generated units are pure machine-code recompilations with no file system I/O, no command-line argument parsing, no debug/dev mode flags, and no direct asset loading from disk.

All file loading in the recompiled game's context appears to be handled entirely outside the generated units — either by the host application (which does not exist at `C:\Dev\ps2-web-recomp\sdbz\host\` — that path was not found) or by a separate runtime layer not present in the generated C++ units.

## Negative Search Results — Generated Units

| Search Category | Terms Searched | Result |
|---|---|---|
| Direct file loading | `direct_load`, `file_load`, `load_from_disk`, `override_path`, `load_from`, `load_direct` | 0 matches |
| File system primitives | `fopen`, `fread`, `read_file`, `file_exists` | 0 matches |
| Command-line arguments | `--disc`, `--window`, `--debug`, `--dev`, `--direct` | 0 matches |
| Debug/dev modes | `debug_mode`, `dev_mode`, `test_mode`, `bypass_archive` | 0 matches |
| Asset loading by name | `.stg`, `.mdl`, `.hit`, `.pix` resolution/loading | 0 matches |
| General file I/O | `open`, `read`, `seek`, `disk`, `path`, `asset`, `rom`, `dvd`, `cdvd` | 0 matches as file-loading context |

## Native C++ Implementation — Direct Loading Path

### `source\platform\sdl_main.cpp` — Command-line parsing and archive loading

| Aspect | Implementation | Lines |
|---|---|---|
| Command-line args | `--mesh`, `--info`, `--game`, positional input path | 82–117 |
| Archive auto-detection | Checks INFO.DAT/GAME.DAT next to input path | 128–173 |
| Archive fallback | Walks up from exe dir to find `assets/original/INFO.DAT` + `GAME.DAT` | 175–214 |
| Direct .pix load | `read_file()` + `decode_first_fpix_texture()` | 270–285 |
| Direct .mdl load | `read_file()` + `parse_fmdl()` with sibling texture/motion discovery | 287–372 |
| Direct .hit load | `read_file()` + `parse_hit()` | 353–369 |
| Direct .stg load | `list_files()` + `parse_stg()` + sibling `.mdl`/`.hit`/`.pix` discovery | 370–556 |
| Motion package load | `read_file()` + `parse_rfsm()` | 577–603 |
| Font load | Archive entry or `.fon` next to input | 606–631 |
| Stage textures | Direct `.pix` read or archive entry lookup | 727–767 |
| Default player | `ply/p01/p01a.mdl` from archive via path match | 995–1026 |

**Key native behaviors:**
- Command-line `--info` and `--game` explicitly specify INFO.DAT/GAME.DAT paths
- Auto-detection: checks input directory, then walks up from executable to `assets/original/`
- Archive loading via `s_archive::from_encoded_info()` (nibble-swap + key-subtraction codec)
- Direct disk loading: `.stg`, `.mdl`, `.hit`, `.pix`, `.fon`, `.pmt` files loaded via `read_file()` and sibling file discovery by stem name
- Fallback chain: explicit args → auto-detect next to input → `assets/original/` fallback → no archive (error)

### `source\interface\vfs_overlay.cpp` — Archive VFS browser

| Aspect | Implementation | Lines |
|---|---|---|
| VFS overlay | Draws archive entries from INFO.DAT/GAME.DAT | 13–61 |
| Navigation | A/Enter open/load, B/Backspace parent, D-pad navigate, Esc close | 56 |

### `source\interface\debug_menu\debug_menu.cpp` — Debug menu

| Aspect | Implementation | Lines |
|---|---|---|
| Archive VFS menu item | Opens VFS browser when archive loaded | 31–32, 164–173 |
| No bypass/archive flags | No `debug_mode`, `dev_mode`, `test_mode`, `bypass_archive` | N/A |

## Comparison Table — Native vs Generated

| Aspect | Native Implementation | Generated Units | Status |
|---|---|---|---|
| Command-line parsing | `--info`, `--game`, `--mesh`, positional input | Not present | ❌ Absent |
| Archive auto-detection | Input dir, exe-dir walkup, `assets/original/` fallback | Not present | ❌ Absent |
| INFO.DAT/GAME.DAT loading | `from_encoded_info()`, sector-based reads | Not present | ❌ Absent |
| Direct .stg loading | `read_file()` + `parse_stg()` + sibling discovery | Not present | ❌ Absent |
| Direct .mdl loading | `read_file()` + `parse_fmdl()` | Not present | ❌ Absent |
| Direct .hit loading | `read_file()` + `parse_hit()` | Not present | ❌ Absent |
| Direct .pix loading | `read_file()` + `decode_first_fpix_texture()` | Not present | ❌ Absent |
| Archive VFS browser | `vfs_overlay.cpp` with entry listing | Not present | ❌ Absent |
| Debug menu | Stage/motion/effect/collision/templates/archive VFS | Not present | ❌ Absent |
| Dev/debug/bypass flags | None (only Archive VFS menu item) | Not present | ❌ Absent |
| `s_archive_entry` lookup | `find_archive_entry_by_path()` full VFS path match | Not present | ❌ Absent |

## ELF Address Mapping — Not Applicable

No ELF addresses were found in generated units for direct loading because no file I/O code exists in the units. The ELF research documents (`ps2-elf-spawn-routine.md`, `mips-disassembly-analysis.md`, `eu-beta-elf-analysis.md`) document code addresses for spawn/placement/fighter logic but **do not document** archive loading or file system routines.

## Where Direct Loading Might Exist

Since no file loading code exists in the 240 generated units:

1. **Host application layer:** The path `C:\Dev\ps2-web-recomp\sdbz\host\` does not exist. The host application that loads and executes the generated units may contain file I/O, but it is not part of the generated units or available in this workspace.

2. **Unregistered VA gaps:** 160 KB of unregistered VA space (unit_00CA–00CD, unit_00CF–00D1, unit_00E6, unit_00F4) has no generated code. Archive loading or file I/O could theoretically reside here, but no code is available to analyze.

3. **Runtime layer outside units:** The recompiled game may use a separate runtime (EE/IOP kernel, DVD/CDVD driver, file system) that loads archives before transferring control to the generated units. This is outside the scope of the generated unit files.

## Recommendations for Bypassing/Replacing Archive Loading

To load game data directly, bypassing INFO.DAT/GAME.DAT:

1. **Native C++ path (already works):** `sdl_main.cpp` already supports direct disk loading of `.stg`, `.mdl`, `.hit`, `.pix` files via `read_file()` + sibling discovery. No archive required if files are extracted to disk.

2. **Command-line control:** Use `--info` and `--game` to explicitly specify archive paths, or omit them to trigger auto-detection/fallback.

3. **Generated units path:** No bypass mechanism exists in the generated units. The units expect data to already be in memory (passed via registers/arguments). To bypass archives at the unit level, you would need to:
   - Identify which unit receives archive data as input arguments
   - Replace that data with direct file-loaded data before calling the unit
   - This requires understanding the unit calling convention ($a0 = stage object, $a1 = fighter object, etc.)

4. **Archive entry lookup replacement:** The native `find_archive_entry_by_path()` matches full VFS paths (e.g., `ply/p01/p01a.mdl`). To bypass, load files directly from disk using the same stem-based sibling discovery already implemented in `sdl_main.cpp`.

*Read-only analysis. No files in ps2-web-recomp were modified.*