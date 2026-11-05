# Recomp Animation Controller Search — SuperDragonBallZ

Source: `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`
Read-only analysis. No files in ps2-web-recomp were modified.

## Summary

**No animation controller class was found** in any of the 240 generated unit files. The generated units are machine-code recompilations that strip all high-level symbolic names. No text identifiers related to animation exist anywhere in the generated code.

## Negative Search Results

The following search terms returned **zero matches** across all `unit_*.cpp` files:

| Search Category | Terms Searched | Result |
|---|---|---|
| Animation controller | animation, anim, mot2, slerp, interpolat, quat, clip, frame, fps, playback, sprite | 0 matches |
| Animation state | idle, walk, run, jump, fall, land, state | 0 matches |
| Clip transition | fade, crossfade, transition, blend | 0 matches |
| MOT2 sampling | mot2, sampling, current_frame, frame_count | 0 matches |
| Animation struct fields | clip_index, anim_time, loop_flag, playback_speed | 0 matches |

## Instruction-Pattern Findings (Generic, Not Animation-Specific)

The only float constants found throughout the units are generic IEEE-754 values used for general math/physics, not specifically animation:

| Constant | Hex | Meaning | Units Found In |
|---|---|---|---|
| 1.0f | `0x3F800000` | Unity float | unit_0000, 0001, 0002, 0003, 0004, 0006, 0012, 001D, 0020, 0021, 0026, and many others |
| 0.5f | `0x3F000000` | Half float | unit_0002, 0003, 0004, 0012, 0013, 0021, 0023, 0025, 0026, and others |
| 2.0f | `0x40000000` | Two float | unit_0000, 0001, 0002, 0003, 0005, 0006, 000A, 0020, 0023, 0025, 0026, and others |
| 0.0f | `0x00000000` | Zero float | unit_0000, 0002, and others |

These constants are ubiquitous across all units (normalization, physics, vector math) and cannot be attributed to an animation controller specifically.

## Fighter Object Field +0x528 (Possible Animation State — Unconfirmed)

The spawn research (`SLUS_spawn_research_log.md`, `recomp-spawn-callers.md`) references a caller loop gated on `fighter+0x528 == 29`. This offset **could** be an animation state field, but:

- It was identified only as a numeric gate value (29), not confirmed as animation state
- No animation-related code accesses `+0x528` in any unit file
- The field's semantic meaning is unresolved (could be animation state, AI state, behavior state, or something else entirely)

| Offset | Object | Possible Semantics | Evidence | Confidence |
|---|---|---|---|---|
| `+0x528` | Fighter | Animation state? (value 29 used as gate) | Caller loop gate in unit_0050, unit_0056, unit_0055, unit_005F, unit_0066, unit_0067 | Low — unconfirmed |

## Confirmed Fighter Animation-Related Fields (From Prior Research)

From the game class reconstruction and spawn research, the following fighter fields are confirmed but **not animation-specific**:

| Offset | Field | Semantics | Confirmed By |
|---|---|---|---|
| `+0x00` | `m_vtable` | vtable pointer | unit_004E spawn routine |
| `+0x510` | `m_spawn_position` | spawn vec4 [x,y,z,w] | adjacent lifecycle (0x1CD9D0) |
| `+0x520` | `m_flags` | flags bitmask | unit_004E adjacent (`fighter+1312`) |
| `+0x534` | `m_side_controller_ptr` | pointer to side/controller sub-object | unit_0032 helper call |
| `+0x538` | `m_vtable_table_index` | vtable table index | adjacent lifecycle |
| `+0x53c` | `m_vtable_table_index_mirror` | mirrors +0x538 | adjacent lifecycle |
| `+0x534+0x08` | `m_side_selector` | binary side selector (0=P1, 1=P2) | unit_0032 helper |

**None of these are animation controller fields.** No `clip_index`, `anim_time`, `playback_speed`, `loop_flag`, `current_frame`, or `frame_count` fields exist in the reconstructed layout.

## Vtable Pointer at +0x00

Every object (stage, fighter, session) confirmed to have a vtable pointer at `+0x00`, but no animation controller vtable or vtable slots were identified:

| Object | +0x00 Field | Confirmed In |
|---|---|---|
| s_game_stage | m_vtable | unit_004E line 3994 |
| s_fighter | m_vtable | unit_004E lines 4181/4192 |
| s_game_session | m_vtable | inferred from spawn callers |

No dedicated animation controller object with its own vtable was found.

## Conclusion

**The animation controller class does not exist as an identifiable entity in the generated units.** Possible explanations:

1. **Animation code is inlined** within fighter/state-machine units rather than a separate controller class
2. **Animation data (MOT2 clips, PMT names) lives in data files** (STG, GAME.DAT, INFO.DAT) rather than code, and the controller logic may be in unregistered VA gaps (unit_00CA–00CD, unit_00CF–00D1, unit_00E6, unit_00F4)
3. **The recompilation stripped all symbolic names**, making animation-specific code indistinguishable from general math/vector code by text search

## Recommendations for Continuing Animation RE

1. **Search data files, not code:** MOT2 animation format and PMT clip names likely exist in extracted STG/GAME.DAT/INFO.DAT assets, not in generated unit code.
2. **Check unregistered VA gaps:** unit_00CA–00CD (0x00428000–0x00437FFF), unit_00CF–00D1 (0x0043C000–0x00447FFF), unit_00E6 (0x00498000–0x0049BFFF), unit_00F4 (0x004CD000–0x004D3FFF) have no generated code and may contain animation logic.
3. **Examine fighter state machine units** (unit_0032, unit_0033, unit_0034) for state-transition logic that could double as animation state transitions.
4. **Look for `+0x528` access patterns** across all units to determine if it's animation state — currently only identified as a numeric gate value (29) in caller loops.

*Read-only analysis. No files in ps2-web-recomp were modified.*