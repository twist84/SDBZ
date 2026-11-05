# Recompiled UI code search — Super Dragon Ball Z

Search for UI/menu-related code in `ps2-web-recomp/sdbz/generated/SuperDragonBallZ/`
generated MIPS interpreter units. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Search scope

All generated units (`unit_0000.cpp` through `unit_00DF.cpp`, plus `unit_00E0`–`unit_00EF`).
Total units: ~240. Units outside prior ranges (0000–004F and 00E0–00EF): **137 units (0050–00DF)**.

## Search keywords — NEGATIVE RESULTS

No high-level UI/menu string literals exist in any generated unit. This is expected because these are low-level MIPS interpreter decompilations — UI code is implemented as register operations, float loads/stores, and memory accesses without high-level keyword strings.

| Keyword(s) searched | Matches | Notes |
|---|---|---|
| menu, MENU | 0 | — |
| character_select, vs_screen, title_screen, stage_select | 0 | — |
| debug_menu, debug_option, home_key | 0 | — |
| vfs, VFS, archive, ARCHIVE | 0 | — |
| vfs_overlay, archive_browser | 0 | — |
| hud, HUD, draw_hud, hud_info | 0 | — |
| draw, DRAW | 0 | — |
| text, TEXT, font, FONT, sprite, SPRITE | 0 | — |
| render, RENDER | 0 | — |
| cursor, CURSOR, highlight, HIGHLIGHT | 0 | — |
| confirm, CONFIRM, cancel, CANCEL | 0 | — |
| option, OPTION | 0 | — |
| menu_navigation, d_pad_menu, enter_select, back_escape | 0 | — |
| title, TITLE | 0 | — |
| select, SELECT | 0 | — |
| character, CHARACTER | 0 | — |
| stage, STAGE | 0 | — |
| vs_screen, VS_SCREEN | 0 | — |
| overlay, OVERLAY | 0 | — |
| browser, BROWSER | 0 | — |

## False positive matches

The following search terms produced matches but none are UI-related:

| Term | Actual match | Reason |
|---|---|---|
| pad, PAD | `mmi::padduw`, `mmi::paddub`, `mmi::paddh` | SIMD "parallel add" instructions, not controller input |
| key, KEY | `fp::key(c.f[N])` | Float-to-int conversion for FPU compare, not keyboard input |
| draw, DRAW | Substring matches in other words | No actual draw/render function names |

## Machine-code level UI analysis

Given the complete absence of UI string literals, UI/menu functionality cannot be identified through keyword search alone. These generated units are MIPS instruction emulators — each `unit_XXXX.cpp` file contains handlers for MIPS CPU instructions at a specific VA range. Game logic (including UI/menus) is implemented through the interaction of these instruction handlers with game data in memory, not as identifiable UI functions within individual units.

### What would be needed to find UI code

To identify UI/menu code at this decompilation level, one would need to:

1. **Trace VAs to UI functions**: Map specific VA ranges to known UI functions from the original ELF binary (SLES_541.61), then find which `unit_XXXX.cpp` files contain those VAs.

2. **Identify screen coordinate math**: Look for patterns of screen-space coordinate manipulation (e.g., values in 0–640 or 0–480 ranges for PS2 display).

3. **Identify text/font data**: Look for glyph data tables or character code handling in the 0x00400000–0x00500000 data region.

4. **Identify menu state machines**: Look for state variable updates (menu index, selection cursor position) and menu transition logic.

None of these patterns were specifically searched for or identified in this analysis.

## Units outside prior ranges

137 units exist in the 0050–00DF range that were not previously analyzed for class layouts or vtable implementations. None of these contain UI string literals either.

## Summary

| Search category | Result |
|---|---|
| Menu keywords ("menu", "character_select", "vs_screen", "title_screen", "stage_select") | 0 matches |
| Debug menu keywords ("debug_menu", "debug_option", "home_key") | 0 matches |
| VFS overlay keywords ("vfs", "archive_browser", "vfs_overlay") | 0 matches |
| HUD keywords ("hud", "draw_hud", "hud_info", "draw", "render") | 0 matches |
| UI input keywords ("menu_navigation", "d_pad_menu", "enter_select", "back_escape", "input", "button", "controller") | 0 matches (only `fp::key()` FPU conversion and `mmi::padd*` SIMD instructions) |
| UI display keywords ("text", "font", "sprite", "cursor", "highlight", "confirm", "cancel", "option") | 0 matches |
| Units with UI code identified | None |

**Conclusion:** No UI/menu code is identifiable in the generated decompiled units at the string-literal level. The decompiled units are low-level MIPS instruction emulators that do not contain high-level game system names, UI element names, or menu state names. UI functionality, if present, would need to be traced through VA-to-function mapping against the original ELF binary rather than through keyword search in these generated files.
