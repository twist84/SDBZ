# Recompiled Code — Additional Class Types Search Results

## 1. Summary

**No additional class types were found** beyond the 4 already documented classes (s_game_stage, s_fighter, s_game_session, s_fighter_side) in any of the 240 generated EE units (`unit_0000.cpp` through `unit_00F8.cpp`) or the 9 VU1 microcode files. The class validation document (`recomp-class-validation.md`) already confirmed that no additional fields were discovered on the validated objects, and this search confirms no entirely new class types exist either.

The generated EE code only contains these 4 class types with vtable pointers at +0x00:

| Class | Sizeof | Vtable at +0x00 | Confirmed fields |
|-------|--------|-----------------|-----------------|
| s_game_stage | 0x28 | Yes | +0x08 stage_id, +0x24 variant_bit |
| s_fighter | 0x540 | Yes | +0x510 spawn_position, +0x520 flags, +0x534 side_controller_ptr, +0x538/+0x53c vtable index |
| s_game_session | 0x68 | — | +0x58 stage ptr, +0x60 fighter1 ptr, +0x64 fighter2 ptr |
| s_fighter_side | 0x0c | — | +0x08 side_selector (unconfirmed) |

## 2. Search Results by Category

### 2.1 Camera Class — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bcamera\b` | No matches |
| `\bCamera\b` | No matches |
| `\bcam\b` | No matches |

No camera position fields, camera offset fields, or camera vtable references exist in the generated EE code.

### 2.2 Effect Class — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bCEffect\b` | No matches |
| `\bCEBZ\b` | No matches |
| `\beffect\b` | No matches |
| `\bEffect\b` | No matches |

The ELF research references effect manager addresses (0x004CA870, 0x004CEF18, 0x004CEF50), but no corresponding effect class or CEffectMgr/CEBZ references exist in the generated EE code. These addresses fall outside the decompiled EE VA ranges or are IOP-side code.

### 2.3 Audio Class — NOT FOUND

| Search term | Result |
|-------------|--------|
| audio manager | No matches (see separate audio search) |
| voice class | No matches |
| \baudio\b | No matches |
| \bvoice\b | No matches |

As documented in `recomp-audio-code-search.md`, no audio code exists in the EE units. Audio runs on the IOP.

### 2.4 Animation Controller Class — NOT FOUND

| Search term | Result |
|-------------|--------|
| \banim\b | Only VU1 microcode `clip_value`/`r.clip` (VU register clipping), not animation |
| \bAnim\b | No matches |
| \bclip\b | Only VU1 microcode `clip_value`/`r.clip` (VU register clipping), not animation clips |
| \bClip\b | No matches |

No animation state, clip controller, or animation controller class exists in the generated EE code.

### 2.5 Other Classes with Vtable at +0x00 — NOT FOUND

A systematic scan of all generated units for patterns consistent with a class having a vtable pointer at +0x00 plus multiple field accesses found **only the 4 already-documented classes**. The class validation document explicitly states: "No memory accesses were found at offsets not predicted by the reconstructed classes on s_fighter, s_game_session, or s_game_stage" and "No additional fields were discovered."

## 3. Files Searched

All 249 files in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`:
- 240 EE units: `unit_0000.cpp` through `unit_00F8.cpp` (VA `0x00100000`–`0x004E0000`)
- 9 VU1 microcode units: `vu1_000.cpp` through `vu1_008.cpp`
- `table.cpp` (unit address mapping)
- `units.hpp` (unit function declarations)

Plus the research validation document: `research/agents/recomp-class-validation.md`

## 4. Conclusion

The ps2-web-recomp EE decompilation contains exactly 4 class types. Camera, effect, audio manager, and animation controller classes do not exist in the decompiled EE code — they are likely IOP-side code or reside in binary regions not covered by this decompilation effort.
