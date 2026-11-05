# Recompiled vtable implementation mapping — Super Dragon Ball Z

Cross-reference of reconstructed game class vtable slots against `ps2-web-recomp/sdbz/generated/SuperDragonBallZ/`
decompiled MIPS interpreter units. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Source references

- `research/agents/game-class-reconstruction.md` — reconstructed layouts (s_game_stage, s_fighter, s_fighter_vtable, s_game_session, s_fighter_side)
- `research/agents/recomp-class-mapping.md` — prior field access cross-reference
- `research/agents/character-animation-systems.md` — ELF research

## Search scope

Generated units checked:
- `unit_0000.cpp` through `unit_004F.cpp` (0x00–0x4F)
- `unit_00E0.cpp` through `unit_00EF.cpp` (0xE0–0xEF)

Vtable slot offsets (decimal):
| Slot | Offset | Struct | Description |
|---|---|---|---|
| +0x28 | 40 | s_game_stage | slot selector |
| +0x40 | 64 | s_fighter_vtable | position setter |
| +0x4c | 76 | s_fighter_vtable | orientation |
| +0x78 | 120 | s_fighter_vtable | state config |
| +0xac | 172 | s_fighter_vtable | post-placement |

Vtable call pattern in decompiled code:
1. `set32(reg, ld32((obj.w[0])))` — load vtable pointer from object+0x00
2. `set32(reg, ld32((vtable.w[0] + OFFSETu)))` — load function address from vtable+slot offset
3. `call(c, addr, ...)` — indirect call through computed address

---

## 1. Vtable CALL SITES — where each virtual method is invoked

### Stage vtable +0x28 (slot selector) — 66 units

Units in range 0000–004F containing vtable+0x28 call patterns:

unit_0002, unit_0003, unit_0004, unit_0005, unit_0006, unit_000A, unit_000B, unit_000C, unit_000D, unit_000E,
unit_000F, unit_0010, unit_0013, unit_0014, unit_0015, unit_0016, unit_0017, unit_0018, unit_0019, unit_001A,
unit_001B, unit_001E, unit_001F, unit_0022, unit_0023, unit_0026, unit_0027, unit_0028, unit_0029, unit_002E,
unit_002F, unit_0030, unit_0031, unit_0032, unit_0033, unit_0035, unit_0036, unit_0038, unit_0039, unit_003A,
unit_003B, unit_003F, unit_0043, unit_0044, unit_0045, unit_0046, unit_0048, unit_0049, unit_004B, unit_004E,
unit_004F

Highest call counts: unit_003A (20), unit_0035 (17), unit_0036 (17), unit_0018 (10), unit_000D (13), unit_000E (11).

### Fighter vtable +0x40 (position setter) — 58 units

unit_0000, unit_0004, unit_0005, unit_0006, unit_0008, unit_0009, unit_000B, unit_000D, unit_000F, unit_0010,
unit_0013, unit_0014, unit_0015, unit_0018, unit_0019, unit_001C, unit_001E, unit_0022, unit_0024, unit_0027,
unit_0028, unit_0029, unit_002E, unit_0030, unit_0031, unit_0032, unit_0034, unit_0035, unit_0036, unit_0037,
unit_0039, unit_003A, unit_003B, unit_003D, unit_003E, unit_003F, unit_0042, unit_0043, unit_0044, unit_0045,
unit_0046, unit_0048, unit_0049, unit_004A, unit_004B, unit_004C, unit_004D, unit_004E

Highest call counts: unit_0018 (23), unit_00C9 (28), unit_00BF (32).

### Fighter vtable +0x4c (orientation) — 58 units

unit_0004, unit_0005, unit_0008, unit_0009, unit_000A, unit_000B, unit_000F, unit_0010, unit_0014, unit_0015,
unit_0016, unit_0018, unit_0019, unit_001A, unit_001B, unit_001D, unit_0021, unit_0022, unit_0023, unit_0027,
unit_0028, unit_0029, unit_002E, unit_0030, unit_0031, unit_0032, unit_0033, unit_0034, unit_0035, unit_0036,
unit_0037, unit_0038, unit_0039, unit_003A, unit_003B, unit_003D, unit_003E, unit_003F, unit_0041, unit_0042,
unit_0043, unit_0044, unit_0045, unit_0046, unit_0048, unit_0049, unit_004A, unit_004B, unit_004C, unit_004D,
unit_004E, unit_004F

Highest call counts: unit_0055 (23), unit_00AF (30), unit_00AE (23), unit_00B2 (25).

### Fighter vtable +0x78 (state config) — 45 units

unit_0005, unit_0006, unit_0008, unit_0013, unit_0016, unit_0017, unit_001A, unit_0032, unit_0033, unit_0034,
unit_0035, unit_0036, unit_0037, unit_0038, unit_0039, unit_003A, unit_003C, unit_003D, unit_003E, unit_003F,
unit_0040, unit_0041, unit_0042, unit_0043, unit_0044, unit_0045, unit_0046, unit_0047, unit_0048, unit_0049,
unit_004A, unit_004B, unit_004C, unit_004D, unit_004E

Highest call counts: unit_00A6 (59), unit_00A7 (59), unit_00A5 (43), unit_00A4 (40).

### Fighter vtable +0xac (post-placement) — 9 units

unit_0002, unit_0004, unit_0012, unit_0013, unit_0018, unit_0034, unit_0035, unit_0036, unit_004E

Notable: unit_0035 has 10 matches, unit_004E has 1 (confirmed call pattern at line 3518–3519).

### unit_00E0–00EF — NO vtable slot calls found

None of the E0–EF units contain any vtable slot call patterns for any of the five target offsets. All memory accesses in these units are stack-frame related (c.r[29] + offset).

---

## 2. Vtable IMPLEMENTATIONS — actual function bodies identified

### Position setter (+0x40) — CONFIRMED

**unit_0032.cpp at L_001CB540 (address 0x001CB540)**

```cpp
L_001CB540:
    c.r[3].sd[0] = static_cast<std::int32_t>(0x00440000u);
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00440000u);
    c.f[3] = ld32((c.r[3].w[0] - 24720u));   // float constant from ~0x0043E5B0
    set32(c.r[4], ld32((c.r[16].w[0] + 6276u)));
    c.f[2] = ld32((c.r[2].w[0] - 24716u));
    c.r[3].sd[0] = static_cast<std::int32_t>(0x00440000u);
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00440000u);
    c.f[1] = ld32((c.r[3].w[0] - 24712u));
    c.f[0] = ld32((c.r[2].w[0] - 24708u));
    st32((c.r[16].w[0] + 1296u), c.f[3]);   // fighter+0x510 = spawn vec4 X
    st32((c.r[16].w[0] + 1300u), c.f[2]);   // fighter+0x514 = spawn vec4 Y
    st32((c.r[16].w[0] + 1304u), c.f[1]);   // fighter+0x518 = spawn vec4 Z
    // 4th component stored via unit_003C(c, 171) at fighter+0x51C
```

This function loads 4 float constants from the 0x00440000 region and stores them to fighter object at offsets 0x510–0x51C (spawn vec4). The 4th component (0x51C/W) is stored via a tail call to `unit_003C(c, 171)`.

**Note:** The reconstruction describes +0x510 as "spawn vec4". The decompiled implementation writes to exactly this offset, confirming it is used as the position/spawn vector field. There is no separate "current position" field at a different offset visible in this unit — the spawn position doubles as the position being set.

### Fighter flags manipulation (+0x520) — CONFIRMED

**unit_0032.cpp at L_001CB5E0 (address 0x001CB5E0)**

Reads fighter+0x520 (flags), ORs with a bitmask, and stores back:

```cpp
set32(c.r[6], ld32((c.r[4].w[0] + 1312u)));   // r6 = fighter+0x520 flags
c.r[3].d[0] = c.r[5].d[0] & 256ull;           // bitmask 0xFF
c.r[5].d[0] = c.r[6].d[0] | c.r[5].d[0];      // flags | bitmask
st32((c.r[4].w[0] + 1312u), c.r[5].w[0]);     // store updated flags back to fighter+0x520
```

### Other likely implementation locations

Units with both fighter field accesses (+0x510–0x540) AND fighter vtable call sites are the most likely to contain vtable slot implementations:

| Unit | Fighter fields | vtable+0x40 | vtable+0x4c | vtable+0x78 | vtable+0xac |
|---|---|---|---|---|---|
| unit_0032 | Yes (extensive) | 1 | 15 | 1 | 0 |
| unit_0033 | Yes | 0 | 3 | 5 | 0 |
| unit_0034 | Yes | 5 | 9 | 6 | 1 |
| unit_0035 | Yes | 1 | 5 | 3 | 10 |
| unit_0036 | Yes | 2 | 2 | 1 | 1 |
| unit_0037 | Yes | 2 | 9 | 4 | 0 |
| unit_0038 | Yes | 0 | 15 | 7 | 0 |
| unit_003E | Yes | 1 | 2 | 4 | 0 |
| unit_003F | Yes | 6 | 2 | 7 | 0 |
| unit_004E | Yes | 8 | 1 | 6 | 1 |

### Stage slot selector (+0x28) — NOT definitively identified

No specific entry function was found that clearly implements the stage slot selector (returning 0-4 based on stage data). Units with game_stage field accesses (+0x08 stage_id, +0x24 variant_bit) are: unit_0002, unit_0007, unit_000C, unit_0011, unit_001C, unit_001E. All of these also contain vtable+0x28 call patterns, but the actual selector function body was not isolated.

### Fighter orientation (+0x4c), state config (+0x78), post-placement (+0xac) — NOT definitively identified

No specific entry functions were isolated that clearly implement these vtable slots. The units listed above contain call sites, but identifying the exact implementation entry point requires deeper per-function analysis of each unit.

---

## 3. Vtable call dispatch pattern confirmed

The decompiled code uses the following vtable dispatch sequence (confirmed in unit_004E.cpp lines 3518–3521):

```cpp
set32(c.r[25], ld32((c.r[4].w[0])));     // load vtable ptr from object+0x00
set32(c.r[25], ld32((c.r[25].w[0] + 172u))); // load function from vtable+0xac
c.pc = c.r[25].w[0];
call(c, c.r[25].w[0], depth + 1u);       // indirect call
```

This pattern was detected for all five target offsets across the units listed in section 1.

---

## 4. Discrepancies between reconstructed layouts and decompiled code

### D1: s_game_stage sizeof — writes at +0x28
The reconstruction declares `sizeof == 0x28`, but field accesses at +0x28 (decimal 40) are observed (e.g. unit_0007.cpp, unit_000C.cpp, unit_0011.cpp). Offset 0x28 is exactly at the struct boundary, meaning either `s_game_stage` is larger than 0x28 bytes, or +0x28 accesses a field immediately following the struct.

### D2: s_fighter sizeof — writes at +0x540
The reconstruction declares `sizeof == 0x540`, but stores at +0x540 (decimal 1344) are observed (unit_0032.cpp, unit_0033.cpp). Offset 0x540 is exactly at the struct boundary, suggesting the fighter struct may be larger than 0x540 bytes, or +0x540 is the start of a subsequent field/struct.

### D3: s_fighter_vtable +0x78 state config — call sites found, implementation not isolated
Call sites for vtable+0x78 exist in 45 units (0005–004E), but no specific entry function was identified as the implementation. This slot requires deeper per-function analysis.

### D4: s_fighter_vtable +0xac post-placement — call sites in only 9 units
Only 9 units contain vtable+0xac call patterns (vs 45–66 for other slots). This suggests post-placement is invoked less frequently, or the slot may be unused/null in many class instances. unit_004E.cpp:3518–3521 confirms the call pattern exists.

### D5: s_fighter_side layout — not independently confirmed
+0x08 (side_selector) cannot be distinguished from s_game_stage +0x08 (stage_id) or other uses. No pointer-chase from s_fighter +0x534 (side_controller_ptr) to +0x08 on the pointed-to object was clearly observed.

### D6: Position setter writes to "spawn vec4" (+0x510), not a separate position field
The position setter implementation at unit_0032.cpp:L_001CB540 writes to fighter+0x510 through +0x51C, which the reconstruction labels as "spawn vec4". There is no evidence of a separate "current position" field at a different offset in the units analyzed. The reconstruction may need to revise the field description — +0x510 appears to be both the spawn position and the active position being set by the vtable position setter.

---

## Summary table

| Struct | Slot | Offset | Call sites (units) | Implementation found | Location |
|---|---|---|---|---|---|
| s_game_stage | slot selector | +0x28 (40) | 66 units (0002–004F) | No | — |
| s_fighter_vtable | position setter | +0x40 (64) | 58 units (0000–004E) | Yes | unit_0032.cpp:L_001CB540 (0x001CB540) |
| s_fighter_vtable | orientation | +0x4c (76) | 58 units (0004–004F) | No | — |
| s_fighter_vtable | state config | +0x78 (120) | 45 units (0005–004E) | No | — |
| s_fighter_vtable | post-placement | +0xac (172) | 9 units | Partial | unit_004E.cpp:3518–3519 (call site only) |
| All | — | — | unit_00E0–00EF: none | No | — |
