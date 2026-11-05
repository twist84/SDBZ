# PS2 Web Recomp Live Patch-and-Test Workflow (SDBZ)

Build: `C:\Dev\ps2-web-recomp\sdbz\build-clang` (Ninja, Release)
Game: loads `SLES_541.61` via `play_SuperDragonBallZ.cmd`
Artifacts produced: `ps2game.lib`, `ps2runtime.lib`, `ps2run.exe`, `SDBZ.exe`

## 1. Rebuild after editing a generated unit

All incremental rebuilds run inside `build-clang` with Ninja.

```cmd
cd /d C:\Dev\ps2-web-recomp\sdbz\build-clang
ninja
```

`ninja` with no arguments rebuilds only what changed (object → `ps2game.lib` → `ps2run.exe`/`SDBZ.exe`). Because the game code is split into one object per generated unit, editing a single unit recompiles only that unit's `.cpp.obj` and relinks the two executables — typically a few seconds.

Build a specific target:

```cmd
ninja ps2game.lib        # rebuild the game static library only
ninja ps2run.exe         # rebuild the ps2run host executable
ninja SDBZ.exe           # rebuild the SDBZ host executable
```

Full clean rebuild:

```cmd
ninja clean
ninja
```

The generated unit sources live at:

```
C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\unit_XXXX.cpp
```

and are compiled into `CMakeFiles\ps2game.dir\sdbz\generated\SuperDragonBallZ\`.

## 2. Run the game

From `C:\Dev\ps2-web-recomp\sdbz`:

```cmd
play_SuperDragonBallZ.cmd
```

`play_SuperDragonBallZ.cmd` sets `PS2_CONFIG=ps2run.ini`, resolves the disc argument from `games\SuperDragonBallZ\disc.iso`, then launches:

```
games\SuperDragonBallZ\disc\ps2run.exe games/SuperDragonBallZ/disc/SLES_541.61 [--disc games/SuperDragonBallZ/disc.iso] --window
```

`ps2run.exe` is copied into both `build-clang\` and `games\SuperDragonBallZ\disc\`. Window/video/audio settings are in `ps2run.ini` (close the game before editing it; hotkeys write settings back).

A copy of the freshly built executable is also at `build-clang\SDBZ.exe` and `build-clang\ps2run.exe`.

## 3. Identify which unit to edit for a given VA address

The game's recompiled code is split into 16 KiB (`0x4000`) VA chunks, one unit per chunk, starting at base `0x00100000`. The chunk → unit map is in:

```
C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\table.cpp
```

Example rows:

```
{0x0022C000u, 16384u, &unit_004B, entries_004B},
{0x00230000u, 16384u, &unit_004C, entries_004C},
{0x00234000u, 16384u, &unit_004D, entries_004D},
```

Formula:

```
unit_index = (VA - 0x00100000) >> 14        # divide by 0x4000
edit file   = generated\SuperDragonBallZ\unit_{unit_index:04X}.cpp
```

Worked example for VA `0x00230000`:
- `(0x00230000 - 0x00100000) >> 14` = `0x130000 >> 14` = `0x4C`
- Edit `generated\SuperDragonBallZ\unit_004C.cpp`

Inside a unit, each entry point is a labelled basic block (`L_00230000`, `L_0023005C`, …) reached via the `entries_XXXX` jump table and the `dispatch_entry` switch at the top of `unit_XXXX()`. The VA of an instruction is its label (`L_<VA>`).

Alternative: search `table.cpp` for the VA range containing your address, or grep the unit files for the label `L_<VA>`.

## 4. Test case — modify spawn coordinates in unit_004C.cpp

VA `0x00230000` maps to `unit_004C.cpp` (see §3). In `unit_004C.cpp` around line 5000 (`L_00233AB8` block) a coordinate value is loaded into an FP register:

```cpp
    c.r[2].sd[0] = static_cast<std::int32_t>(0x42480000u);   // 72.0f
    c.f[21] = c.r[2].w[0];
```

`0x42480000` = 72.0f. Changed it to `0x42C80000` (100.0f):

```cpp
    c.r[2].sd[0] = static_cast<std::int32_t>(0x42C80000u);   // 100.0f
    c.f[21] = c.r[2].w[0];
```

Rebuild (incremental — only this unit recompiled):

```cmd
cd /d C:\Dev\ps2-web-recomp\sdbz\build-clang
ninja
# [1/4] Building CXX object ...unit_004C.cpp.obj
# [2/4] Linking CXX static library ps2game.lib
# [3/4] Linking CXX executable ps2run.exe
# [4/4] Linking CXX executable SDBZ.exe
```

Run:

```cmd
cd /d C:\Dev\ps2-web-recomp\sdbz
play_SuperDragonBallZ.cmd
```

Result: the game launched and ran with the modified build without crashing (launched twice — once unmodified, once patched — ~12 seconds each, terminated cleanly). The patch-and-test loop edits → rebuilds → relinks → runs end-to-end. The exact on-screen delta of the 72.0f→100.0f coordinate was not instrumented here; to verify visually, launch both builds and compare the player's spawn/position against a reference.

## 5. Iterate: edit → rebuild → run → observe

```
1. Edit  C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\unit_XXXX.cpp
2. cd C:\Dev\ps2-web-recomp\sdbz\build-clang && ninja        # incremental, seconds
3. cd C:\Dev\ps2-web-recomp\sdbz && play_SuperDragonBallZ.cmd
4. Observe in-game; close window when done
5. Repeat from step 1
```

Tips:
- Only the edited unit's object is recompiled; `ps2game.lib`, `ps2run.exe`, `SDBZ.exe` relink automatically.
- `ps2run.ini` (scaling, window size, field blending) is read at startup — close the game before changing it.
- If a change breaks compilation, `ninja` reports the error in `unit_XXXX.cpp`; fix and rerun `ninja`.
- For a VA not aligned to a `0x4000` boundary, the containing unit is the one whose `[base, base+0x4000)` range covers the VA (`table.cpp`).
- `SDBZ.exe` and `ps2run.exe` share `ps2game.lib`/`ps2runtime.lib`; rebuilding one relinks both.

## Verification performed

- `ninja` in `build-clang` completed successfully (all 12 steps; warnings only, no errors).
- Artifacts present: `ps2game.lib`, `ps2runtime.lib`, `ps2run.exe`, `SDBZ.exe`.
- `play_SuperDragonBallZ.cmd` launched `SLES_541.61` and ran (terminated after ~12 s).
- Test edit `unit_004C.cpp` (72.0f → 100.0f) → `ninja` incremental rebuild → `play_SuperDragonBallZ.cmd` ran patched build without crashing.
