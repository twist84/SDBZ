# Super Dragon Ball Z — Audio, Input, and UI Systems Research Report

**Source worktree:** `C:\Dev\SuperDragonBallZ\.kilo\worktrees\audio-input-ui`
**ELF analyzed:** `C:\Dev\SuperDragonBallZ\assets\original\SLES_541.61` (MIPS32 LE, entry 0x00100008, loaded at vaddr 0x00100000 = file offset 0x80; vaddr = file_offset + 0x000FFF80)
**Research date:** 2026-11-05

---

## ELF Binary Analysis — SLES_541.61 (EU Beta)

### Audio Middleware Identified

The game uses CRI Middleware (ADX/Sofdec) plus custom game audio layers:

| Component | Version | Purpose | String Location |
|-----------|---------|---------|-----------------|
| ADXB/ADXF/ADXT | ADXF v7.36, ADXT v9.61 | ADPCM audio decode/streaming (build Sep 20 2005) | code strings |
| PS2RNA | — | SPU2 voice setup/allocation | code strings |
| Sofdec (MWS) | — | Movie/streaming audio playback | code strings |
| SFX | — | Sound effect playback | code strings |
| CVFS | v2.44 | CRI Virtual File System | code strings |
| libpad | — | PS2 controller input | code strings |

### SLES ELF Address Map (vaddr = file_offset + 0x000FFF80)

#### Audio Functions (CRI Middleware)
| Function | SLES Address | Purpose |
|----------|-------------|---------|
| adxt_Start | 0x004B60D2 | Start ADX stream playback |
| adxt_Stop | 0x004B658A | Stop ADX stream |
| adxt_Pause | 0x004B6B5A | Pause ADX stream |
| adxt_SetOutVol | 0x004B67E2 | Set output volume |
| adxt_SetOutPan | 0x004B66F2 | Set output pan |
| ADXB_DecodeHeader | ~0x004B6xxx | Decode ADX header |
| ADXB_DecodeHeaderAdx | ~0x004B6xxx | Decode ADX file header |
| PS2RNA_SetupVoice | 0x004B97E0 | Setup SPU2 voice |
| sfx_InitHn | 0x004BD6F9 | Initialize SFX handle |
| mwPlyStart | 0x004BB672 | Sofdec start playback |
| mwPlyPause | 0x004BB7C9 | Sofdec pause |
| mwPlySetOutVol | 0x004BBD41 | Sofdec set volume |

#### Game Audio Classes & Data
| Symbol | SLES Address | Purpose |
|--------|-------------|---------|
| CPlayerVoiStrem | 0x004C1720 | Player voice stream class |
| CSEPlayer | 0x004C0610 | SE (sound effect) player class |
| CMenuSoundSetting | 0x004CBB80 | Menu sound settings |
| `ply/p%02d/voi/%s.ase` | 0x004C15F0 | ASE voice bank path pattern |
| snd/comvoi | 0x004CB150 | Common voice bank |
| snd/etcvoi | 0x004CB1B0 | Etc voice bank |
| snd/mmuvoi | 0x004CB210 | Mumu voice bank |

#### AMS Music Paths (_in = intro, _lp = loop)
| Path | SLES Address |
|------|-------------|
| sqm/sel_in.ams / sqm/sel_lp.ams | 0x00496D70 |
| sqm/menu_in.ams / sqm/menu_lp.ams | 0x004992F0 / 0x00499314 |
| sqm/stXXa_in.ams / stXXa_lp.ams | ~0x00496xxx–0x00499xxx |
| sqm/boss_in.ams / boss_lp.ams | ~0x00496xxx |
| sqm/gameover.ams, sqm/cont.ams, sqm/se130.ams | ~0x00496xxx |
| sqm/s_shkn.ams, sqm/s_ngai.ams, sqm/s_kaij.ams | ~0x00496xxx |
| sqm/me00.ams, sqm/me01.ams | ~0x00496xxx |

#### Input
| Symbol | SLES Address | Purpose |
|--------|-------------|---------|
| CPlayerPad | 0x004C1108 | Player pad/input class |
| libpad / PsIIlibpad | 0x004628FC / 0x004628F8 | PS2 pad library |

#### Menu/UI
| Symbol | SLES Address | Purpose |
|--------|-------------|---------|
| CMenuFuncMng | 0x004CB960 | Menu function manager |
| CFuncCommandListMng | 0x004CBC00 | Command list manager (combos) |
| CMenuCommandList | ~0x004CBxxx | Menu command list |
| "SELECT button: Loading save data." | ~0x004xxxx | Pad input handler |
| "Use directional buttons…button to confirm." | ~0x004xxxx | Menu navigation text |

#### Combat Mechanics
| Symbol | SLES Address | Purpose |
|--------|-------------|---------|
| Super Gauge | 0x004CDB70 | Super meter system |
| Super Cancel | 0x004CDED8 | Super cancel mechanic |
| Guard Cancel | 0x004CDF38 | Guard cancel mechanic |
| Rapid Movement Recovery | 0x004CDAC0 | Recovery tech |

#### SPT Scripts
| Path | SLES Address |
|------|-------------|
| spt/ply/pl00.spt through pl30.spt | 0x004CA750 |

#### Fighter State / Audio Event Strings
| String | SLES Address | Purpose |
|--------|-------------|---------|
| CPlayerGray | 0x004C0B20 | Player state class (possibly Guard/Grapple) |
| CPlayerHit… | 0x004C0B30 | Player hit state timer/state |
| CHit…ase | 0x004C0B50 | Hit SE sample reference |

### ELF Sound Event Triggering Analysis

#### Address Type Clarification
All CRI middleware function addresses listed above (adxt_Start, PS2RNA_SetupVoice, sfx_InitHn, etc.) are **string addresses in .rodata** — they point to the null-terminated function name strings used by the CRI error logging system, not to executable code. The actual function code resides elsewhere in the single PROGBITS section (vaddr 0x00100000–0x004FC900). Direct `jal` scans to these addresses returned zero hits, confirming they are data, not code entry points.

#### Section Layout
- Section [4]: PROGBITS, flags 0x7 (WRITABLE+ALLOC+EXECINSTR), vaddr 0x00100000, offset 0x80, size 0x003FC900 — contains both code and data mixed together.
- Entry point: 0x00100008.
- vaddr = file_offset + 0x000FFF80.

#### Audio Event Dispatch Architecture (inferred from ELF)
The original game dispatches audio through these layers:

1. **CRI ADXT layer** (`adxt_Start`, `adxt_Stop`, `adxt_Pause`, `adxt_SetOutVol`, `adxt_SetOutPan`): Streaming music playback with seamless loop support (`adxt_StartFnameRangeLp`, `adxt_StartSeamless`, `adxt_SetSeamlessLp`). Used for AMS `_in`/`_lp` music streams.

2. **PS2RNA layer** (`PS2RNA_SetupVoice`): SPU2 voice allocation and setup. Used for ASE sample playback with per-voice pitch, volume, and pan control.

3. **SFX layer** (`sfx_InitHn`, `SFX_*`): Sound effect handle management for short SE playback.

4. **CSEPlayer** (0x004C0610): Game-level SE player class with `GetObject` method. Manages SE playback instances.

5. **CPlayerVoiStrem** (0x004C1720): Player voice stream class. Handles character voice sample streaming from ASE banks at `ply/p%02d/voi/%s.ase`.

6. **CPlayerPad** (0x004C1108): Player input/pad class. Reads controller state via libpad.

7. **CMenuSoundSetting** (0x004CBB80): Menu sound settings. Text "Set the narration voice to" at 0x004D0D00 indicates per-voice volume control in menus.

#### Fighter Event Audio Triggers (inferred)
Fighter state strings found:
- `CPlayerGray` at 0x004C0B20 — likely a player state class (guard/grapple/damage state)
- `CPlayerHit…` at 0x004C0B30 — player hit state timer
- `CHit…ase` at 0x004C0B50 — hit SE sample reference

These indicate the original game triggers ASE SE samples on fighter events (hit, jump, land, block, guard, dash, tech, super) through the CSEPlayer / PS2RNA voice layer, with sample selection driven by fighter state and SPT scripts (`spt/ply/plXX.spt`).

#### SLUS_214.42 Comparison

- SLUS ELF was inspected by `sdbz_tool elf` but is **not present** in `assets/original/`.
- Known address shift: **SLUS = SLES − 0x2510** (per project documentation).
- All SLES addresses above map to SLUS by subtracting 0x2510.
- Logic appears to match between regions; only addresses differ.

### Audio Event Triggering — Missing in C++ Codebase

The C++ codebase has **no equivalent** of the ELF's audio event dispatch:
- No `CSEPlayer`-like class for SE playback management.
- No `CPlayerVoiStrem`-like class for character voice streaming.
- No `CPlayerPad`-like class for input (SDL3 handles this directly).
- No `CMenuSoundSetting`-like class for menu sound settings.
- No fighter event → audio cue mapping (hit/jump/land/block/guard/dash/tech/super).
- No SPT script-driven audio cue system.
- No per-fighter voice bank loading (`ply/p%02d/voi/%s.ase`).

### SLUS_214.42 Comparison

- SLUS ELF was inspected by `sdbz_tool elf` but is **not present** in `assets/original/`.
- Known address shift: **SLUS = SLES − 0x2510** (per project documentation).
- All SLES addresses above map to SLUS by subtracting 0x2510.
- Logic appears to match between regions; only addresses differ.

---

## Audio Systems

### ASE Sound Banks — `source/core/ase.cpp`, `source/core/ase.hpp`

**Status: parsed only, not played.**

`parse_ase()` reads an ASE sound-effect bank:
- Version field at offset `0x00`; requires exactly `100`, else throws `"bad ASE version"`.
- Header padded to `0x800` bytes: sample count at `0x04`, payload byte count at `0x08`, then repeated `pitch`/`offset` u32 pairs from `0x0C` (8 bytes each).
- Sample payloads live at `0x800 + offset`; each sample's length is `next_offset - rel` (final sample extends to `payload_byte_count`).
- Each sample carries `m_pitch` and raw `m_payload` (Sony PS ADPCM bytes). No decoding happens here — payloads are stored as-is.

Per `research/audio_format.md`: ASE v100, 16 ADPCM bytes → 28 signed samples per frame, sample rate `pitch * 48000 // 4096`, each sample begins with flag 4 and ends with flag 3. The Python exporter decodes each sample once including its final frame; those flags are preserved in the source and do not cause infinite repetition in the WAV.

ELF confirms ASE voice banks at `ply/p%02d/voi/%s.ase` (SLES vaddr 0x004C15F0), with character voice banks `snd/comvoi`, `snd/etcvoi`, `snd/mmuvoi` and player scripts `spt/ply/plXX.spt`.

### AMS Music Streams — `source/core/ams.cpp`, `source/core/ams.hpp`

**Status: parsed only, not streamed.**

`parse_ams()` reads an AMS music stream:
- Version at offset `0x00`; requires exactly `120`, else throws `"bad AMS version"`.
- Pitch at `0x04`, valid ADPCM bytes per channel at `0x08`, unit count at `0x0C`.
- Each unit is `0x20000` bytes containing two `0x10000`-byte stereo blocks; each block = `0x8000` left-channel bytes followed by `0x8000` right-channel bytes.
- **Even/odd block reordering:** logical playback order = even block indices ascending, then odd block indices descending. ADPCM predictor history is preserved per-channel across all reordered blocks.
- Each channel is trimmed to `m_valid_bytes_per_channel` before decoding, then decoded with a dedicated `s_adpcm_decoder` into `m_left_samples`/`m_right_samples`.

Per `research/audio_format.md`: `_in`/`_lp` name suffixes usually distinguish intro vs loop segments; the Python exporter exports them separately, but **the C++ parser does not implement intro/loop segmentation or gameplay-controlled repetition**. AMS is streamed audio, not an action-control script.

ELF confirms `_in`/`_lp` convention at SLES addresses (e.g. `sqm/menu_in.ams` at 0x004992F0, `sqm/menu_lp.ams` at 0x00499314), plus many stage-specific pairs (`stXXa_in`/`stXXa_lp`).

### ADPCM Decoder — `source/core/audio/adpcm.cpp`, `source/core/audio/adpcm.hpp`

**Status: fully implemented, unit-tested.**

`s_adpcm_decoder::decode()`:
- 5 predictor coefficient sets: `{0,0}, {60,0}, {115,-52}, {98,-55}, {122,-60}` (predictor = header nibble `>> 4`, validated `≤ 4`).
- Shift = header `& 15` (validated `≤ 12`); flags byte validated `≤ 7` (flags == 7 forces sample value to 0).
- Each 16-byte frame = 2 header bytes + 14 data bytes = 28 4-bit nibbles (low nibble first), sign-extended from 4 bits.
- Sample = `(signed_nibble * 4096) >> shift + (h1*c1 + h2*c2) / 64`, clamped to `int16` range `[-32768, 32767]`.
- History `(m_history1, m_history2)` carried across frames within a decode call.

### Audio Playback Status — `source/platform/audio.cpp`, `source/platform/audio.hpp`

**`play_buffer` is a stub.** `c_audio_device::play_buffer(const std::int16_t*, size_t)` body is `(void)data; (void)count;` — it does nothing. The class is compiled into the build (CMakeLists.txt:121) but **never instantiated or called** anywhere in the codebase.

In `sdl_main.cpp` (lines 646-659), a raw SDL3 audio device is opened directly (`SDL_AUDIO_S16`, 44100 Hz, mono) but no audio is ever queued (`SDL_QueueAudio` / callback unused), and `c_audio_device` is not used at all.

**Missing audio systems:**
- No ASE sample playback trigger or voice allocation.
- No AMS music streaming or intro/loop (`_in`/`_lp`) sequencing.
- No mixer, no pitch modulation, no SPU2 interpolation/effects/volume emulation.
- `play_buffer` is a no-op stub; no audio output path exists from decoded ASE/AMS PCM to the SDL device.
- No audio-related HUD indicators or debug-menu volume controls.

---

## Audio Pipeline Completion Design

### Target Architecture

The C++ codebase should mirror the ELF's layered design:

```
Application layer (sdl_main.cpp)
  └── c_audio_device (platform/audio.cpp) — SDL3 device wrapper
        └── SDL_QueueAudio / SDL_LockAudio — low-output
              └── s_audio_mixer (new) — mixes voices + music
                    ├── s_ase_voice (new) — per-sample ASE voice (pitch, vol, pan, ADPCM decode state)
                    └── s_ams_stream (new) — AMS music stream (_in/_lp sequencing)
```

### 1. `c_audio_device::play_buffer` Implementation

Replace the no-op with SDL3 queued playback:

```cpp
void c_audio_device::play_buffer(const std::int16_t* data, std::size_t count)
{
    if (!m_impl->device) return;
    SDL_QueueAudio(m_impl->device, data, count * sizeof(std::int16_t));
}
```

Note: SDL3 `SDL_QueueAudio` requires the device to be opened without a callback. The current `sdl_main.cpp` opens the device at lines 647-659 but never uses `c_audio_device`. The platform layer should instantiate `c_audio_device`, open it (44100 Hz, stereo for AMS), and feed mixed PCM via `play_buffer` each frame (or via an SDL audio callback thread).

### 2. ASE Voice Allocation Layer

New class `s_ase_voice`:
- Holds decoded PCM samples (from `s_adpcm_decoder`), current play position, loop flag, volume, pan, pitch ratio.
- `play(sample_index, pitch, volume, loop)` — allocates a free voice, decodes the ASE sample via `s_adpcm_decoder`, starts playback.
- `stop(voice_id)`, `is_playing(voice_id)`, `update(delta)` — advance position, handle voice stealing when all voices are in use.
- Voice pool: fixed count (e.g. 16–32 voices), LRU or priority-based stealing.

ASE voice lifecycle per ELF (`CPlayerVoiStrem`, `CSEPlayer`):
1. Load ASE bank (`parse_ase`).
2. On trigger (e.g. character voice cue), decode selected sample slot via `s_adpcm_decoder`.
3. Resample pitch: output rate = `pitch * 44100 // 4096` (matching the ELF's SPU2 pitch convention).
4. Mix into stereo output buffer with volume/pan.

### 3. AMS Music Stream Layer

New class `s_ams_stream`:
- Holds parsed `s_ams` (decoded left/right PCM samples).
- Manages `_in`/`_lp` segment pairing: load `<name>_in.ams` and `<name>_lp.ams` as a pair.
- Playback state machine: `INTRO` → play `_in` segment to completion → `LOOP` → play `_lp` segment seamlessly repeating.
- Crossfade or sample-accurate seam between intro and loop (no gap).
- Volume control, pause/resume, stop.

ELF confirmation: `adxt_StartFnameRangeLp` / `adxt_StartSeamless` / `adxt_SetSeamlessLp` at SLES addresses (0x004B6xxx) indicate seamless loop support exists in the original ADXT layer.

### 4. Audio Mixer

New class `s_audio_mixer`:
- Maintains voice pool + active music stream.
- Each frame: mix all active voices and music stream into a stereo int16 buffer.
- Apply global volume, master output.
- Call `c_audio_device::play_buffer(mixed_buffer, sample_count)` per frame.

### 5. Audio Triggers from Gameplay

- ASE: on player attack/hit/jump, call `ase_voice.play(sample_slot, pitch, volume)`.
- AMS: on stage load, call `ams_stream.play("sqm/st01a")` which auto-selects `_in` then `_lp`.
- On menu open: `ams_stream.play("sqm/menu")`.

---

## Input Systems

### Keyboard — `source/platform/sdl_main.cpp`

| Key | Action | Context |
|-----|--------|---------|
| W/A/S/D | Move forward/left/back/right | Player controller active |
| Shift (L/R) | Run | Player controller active |
| Space | Jump request | Player controller active |
| P | Toggle player controller | Outside menu/VFS |
| C | Recenter gameplay camera follow offset | Player controller active |
| H | Toggle HIT/capsule debug | Stage loaded |
| E | Toggle effect rendering | Stage loaded |
| T | Toggle templates rendering | Stage loaded |
| F | Refit camera | Outside menu/VFS |
| R | Unstuck / respawn at stage spawn | Player controller active |
| N / B | Next / previous motion clip | Motion loaded, player inactive |
| 1–9 / 0 | Cycle model part groups (shift = reverse, 0 = reset) | Model loaded |
| + / − | Zoom | Player camera (active) or viewer camera |
| PageUp / PageDown | Cycle stages | Stage loaded |
| Home | Toggle debug menu (closes VFS if open) | Any state |
| End | Back one menu level / close menu or VFS root | Any state |
| Escape | Quit application | Any state |

### Mouse — `source/platform/sdl_main.cpp`

| Input | Action |
|-------|--------|
| Left drag | Orbit camera. Player camera → `orbit_camera(player_camera, …)`; viewer camera → yaw + pitch (pitch clamped ±85°) |
| Right / Middle drag | Pan camera target along camera right/up axes, cursor-anchored |
| Wheel | Zoom. Player camera → `zoom_orbit_camera(player_camera, wheel)` (signed orbit distance); viewer camera → scale `camera.m_distance` by `pow(0.88f, wheel)` with cursor-anchored target shift |

### Gamepad (SDL3) — `source/platform/sdl_main.cpp`

- `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_AUDIO)`; `SDL_GetGamepads` / `SDL_OpenGamepad` (lines 706-724).
- **Deadzone 0.18:** `gamepad_axis_to_input()` clamps axis / 32767 to [-1,1], zeroes magnitude ≤ 0.18, remaps `(mag - 0.18) / (1 - 0.18)` with sign preserved (lines 45-56).
- **Left stick** (LEFTX/LEFTY): movement; **LB** (LEFT_SHOULDER): run (lines 2266-2270).
- **Right stick** (RIGHTX/RIGHTY): camera orbit; **RB** held raises zoom-speed multiplier via RIGHTY (lines 2272-2291).
- **A / South**: jump (when playing), menu select/activate (lines 2152, 2167-2169).
- **B / East**: menu back / End (lines 2155-2156).
- **Select (BACK) + Start chord**: toggles debug menu; edge-latched until both released (lines 2100-2127).
- **D-pad**: mapped to SDLK_UP/DOWN/LEFT/RIGHT and pushed into the menu/VFS input path via `push_gamepad_menu_key()` (lines 2138-2165).
- **West (X/Square)**: gamepad equivalent of P — toggle player controller (lines 2171-2217).
- **North (Y/Triangle)**: gamepad equivalent of R — unstuck/respawn (lines 2219-2224).
- Gamepad add/remove handled with ID-matching guards (lines 2078-2093, 2226-2241).

ELF confirms `CPlayerPad` (SLES 0x004C1108) and `libpad`/`PsIIlibpad` (SLES 0x004628FC/0x004628F8) as the original input layer.

---

## Combat Input Extension Design (Beyond Viewer Scope)

The current input handles viewer movement (WASD + gamepad left stick). A fighting game requires directional commands and button combos. ELF confirms `CFuncCommandListMng` / `CMenuCommandList` (SLES 0x004CBC00) and combat mechanics: Super Gauge (0x004CDB70), Super Cancel (0x004CDED8), Guard Cancel (0x004CDF38), Rapid Movement Recovery (0x004CDAC0).

### Proposed Input System

**Directional commands (4-frame input buffer, 60 Hz):**
- Track last N directional inputs (e.g. 12 frames at 60 Hz = 0.2s window).
- Detect: QCF (↓↘→), QCB (↓↙←), DP (→↓↘), HCF (→↓↘), charge back/forward, etc.
- Each command maps to a `s_player_command` with type + required button.

**Buttons (keyboard + gamepad):**
- Keyboard: P (punch), K (kick), H (heavy/guard), C (special/ki), Space (jump/dash).
- Gamepad: A/South (punch), B/East (kick), X/North (heavy), Y/West (special), LB/L1 (guard), RB/R1 (dash).

**Combo system:**
- `s_combo_move { name, directional_command, button, priority, super_gauge_cost }`
- On each frame, check buffered inputs against combo list; highest priority match wins.
- Examples: QCF+P = fireball, DP+K = dragon rush, →→+P = dash punch.

**Input state struct:**
```cpp
struct s_player_input {
    float m_move_x, m_move_forward;      // analog movement
    bool m_run, m_guard, m_jump;          // buttons
    int m_command_dir[4];                 // last 4 directional inputs
    int m_command_frame[4];               // frame each was pressed
    bool m_btn_punch, m_btn_kick, m_btn_heavy, m_btn_special;
};
```

**Implementation point:** `sdl_main.cpp` lines 2249-2293 where `s_player_input` is built each frame. Add command-buffer tracking and combo detection before calling `update_player_controller`.

---

## UI Systems

### HUD Debug Info — `source/interface/hud.cpp`

`draw_hud()` draws when `!debug.open && !vfs_open && has_font`:
- Title "INFO.DAT/GAME.DAT viewer" + model/stage/motion/mesh-filter line.
- Player HUD (when active): move/jump/recenter/HIT hint line; position/velocity/grounded/HIT-object line.
- Part groups 1-9 with active variant index (hidden shown gray).
- Stage toggles: E effect / H HIT-capsule / T templates / F fit camera.
- Bottom-right camera info: perspective/ortho, yaw, pitch, distance, target.
- Suppressed entirely while debug menu or VFS is open.

For combat, HUD should add: Super Gauge bar, health bars (player/CPU), round timer, input command display (last directional input), hit/counter indicators.

### Debug Menu (Home key) — `source/interface/debug_menu/debug_menu.cpp`

`s_debug_menu` with `open`, `selected`, `items`, `stack`.
- **Home** toggles; **End** backs out / closes; **Enter/Space** activates; arrows/D-pad navigate; **0-9a-z** quick-select; max 26 items/page.
- Rows: Stage, Motion, Show effect, Show collision, Show templates, Archive VFS, Fit camera, Quit.
- Left/right adjusts the selected value row (Stage/Motion cycling, toggles on/off).
- "Archive VFS" row opens the archive browser directly (closes menu first).
- `debug_menu_draw()` renders title "DEBUG MENU >…", control hint line, and the item list with selection highlighting.

For audio: add rows "ASE Voice Test", "AMS Music Test", "Master Volume", "SE Volume", "BGM Volume". For combat: add rows "Input Display", "Command Buffer", "Super Gauge", "Frame Data".

ELF confirms `CMenuSoundSetting` (SLES 0x004CBB80) with text "Set the narration voice to" at 0x004D0D00 — original had sound settings in menus.

### VFS Overlay (Archive Browser) — `source/interface/vfs_overlay.cpp`, `sdl_main.cpp`

Rendered by `draw_vfs_overlay()` when `vfs_open`; title "ARCHIVE VFS (INFO.DAT/GAME.DAT)".
- **Breadcrumb navigation:** "Path: comp1 > comp2 > …" or "(root)" at root.
- Entries grouped dirs-first then files, alphabetically sorted; each shown as `[dir]` / `[file]`.
- **Loadable extensions** (handled in sdl_main.cpp VFS Enter handler): `.mdl` → `load_archive_model()`; `.pix` → texture load; `.fon` → font load + glyph upload; `.pmt/.mot` → motion package; `.stg` → full stage load (STG + mdl/hit/pix companions + player spawn); `.hit` → collision load. Everything else: "Archive entry not loadable".
- Controls: Enter/Space/Right opens dir or loads file; Backspace/Left goes up; Esc closes; Home closes VFS.
- Backing data lives in `s_draw_context`: `vfs_archive`, `vfs_game_data`, `vfs_entries`, `vfs_path_components`, `vfs_selected`, `vfs_open`.

For audio: add `.ase` → voice bank preview (play sample), `.ams` → music stream preview (play intro/loop). For combat: no VFS changes needed.

---

## Missing Systems Summary

| Domain | Present | Missing |
|--------|---------|---------|
| ASE parsing | ✅ v100 header + sample table + payload slicing | ❌ sample decoding at playback time, voice allocation, playback triggers |
| AMS parsing | ✅ v120 header, even/odd block reorder, per-channel trim + decode | ❌ streaming, `_in`/`_lp` intro/loop transitions, music sequencing, seamless loop |
| ADPCM decode | ✅ 5 predictors, 4-bit nibbles, int16 clamp, 16→28 samples | ❌ — complete |
| Audio output | ⚠️ SDL device opened (44100 Hz S16 mono) | ❌ `play_buffer` is a no-op stub; `c_audio_device` unused; nothing queued; no mixer |
| Voice allocation | ❌ — not present | ❌ no voice pool, no voice stealing, no pitch/volume/pan per voice |
| Music streaming | ❌ — not present | ❌ no AMS streamer, no intro/loop state machine, no seamless transition |
| Audio triggers | ❌ — not present | ❌ no gameplay cue system for ASE/AMS; no fighter event → SE mapping (hit/jump/land/block/guard/dash/tech/super); no voice sample triggering per fighter state; no SPT script-driven audio cues |
| Menu sound effects | ❌ — not present | ❌ no menu navigation SE (cursor/select/confirm/cancel); no `CMenuSoundSetting`-equivalent volume controls |
| Keyboard input | ✅ WASD+Shift+Space+P+C+H + viewer hotkeys | ❌ no directional command buffer, no button combos, no fighting-game buttons |
| Mouse input | ✅ orbit/pan/zoom (cursor-anchored wheel zoom) | ❌ — complete for viewer scope |
| Gamepad (SDL3) | ✅ deadzone 0.18, debug-menu chord, menu/VFS mapping | ❌ no face-button combat mapping, no combo input |
| HUD debug | ✅ model/stage/motion/player/camera info | ❌ no audio indicators, no super gauge, no health/timer, no command display |
| Debug menu | ✅ Home toggle, 8 rows, gamepad support | ❌ no audio/volume rows, no input/combat debug rows |
| VFS overlay | ✅ archive browser, breadcrumb, loadable extensions | ❌ `.ase`/`.ams` entries report "not loadable"; no audio preview |
