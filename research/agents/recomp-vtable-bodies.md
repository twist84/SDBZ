# Recompiled vtable implementation bodies — Super Dragon Ball Z

Identification of actual function bodies for vtable slots in `ps2-web-recomp/sdbz/generated/SuperDragonBallZ/`
decompiled MIPS interpreter units. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Source references

- `research/agents/game-class-reconstruction.md` — reconstructed vtable slot layout
- `research/agents/recomp-class-mapping.md` — field access cross-reference (confirmed call sites)
- `research/agents/recomp-vtable-implementations.md` — vtable CALL site mapping

## Vtable slot offsets (decimal)

| Slot | Offset | Struct | Description |
|---|---|---|---|
| +0x28 | 40 | s_game_stage | slot selector (returns 0-4) |
| +0x40 | 64 | s_fighter_vtable | position setter |
| +0x4c | 76 | s_fighter_vtable | orientation setter |
| +0x78 | 120 | s_fighter_vtable | state configuration |
| +0xac | 172 | s_fighter_vtable | post-placement |

## 1. Position setter (+0x40) — CONFIRMED IMPLEMENTATION

**unit_0032.cpp at L_001CB540 (VA 0x001CB540), lines 4500-4519**

This function takes a fighter object pointer (in r16) and writes a spawn vec4 from global constant tables to fighter+0x510 through fighter+0x51C.

```cpp
L_001CB540:                                    // unit_0032.cpp:4500
    c.r[3].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4501
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4502
    c.f[3] = ld32((c.r[3].w[0] - 24720u));                   // line 4503 — float from table @0x00439F70
    set32(c.r[4], ld32((c.r[16].w[0] + 6276u)));             // line 4504
    c.f[2] = ld32((c.r[2].w[0] - 24716u));                   // line 4505 — float from table @0x00439F74
    c.r[3].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4506
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4507
    c.f[1] = ld32((c.r[3].w[0] - 24712u));                   // line 4508 — float from table @0x00439F78
    c.f[0] = ld32((c.r[2].w[0] - 24708u));                   // line 4509 — float from table @0x00439F7C
    st32((c.r[16].w[0] + 1296u), c.f[3]);                    // line 4510 — fighter+0x510 = spawn X
    st32((c.r[16].w[0] + 1300u), c.f[2]);                    // line 4511 — fighter+0x514 = spawn Y
    st32((c.r[16].w[0] + 1304u), c.f[1]);                    // line 4512 — fighter+0x518 = spawn Z
    { ... st32((c.r[16].w[0] + 1308u), c.f[0]); ... }        // line 4514 — fighter+0x51C = spawn W (via unit_003C)
    ... return;
```

**Evidence:** Writes 4 floats to fighter+0x510..+0x51C (spawn vec4), exactly matching the reconstructed s_fighter +0x510 field. The 4th component is stored via tail call to `unit_003C(c, 171)`.

---

## 2. Orientation setter (+0x4c) — LIKELY IMPLEMENTATION

**unit_0033.cpp at L_001CF618 (VA 0x001CF618), lines 4640-4685** (3 variants: L_001CF6D0, L_001CFA4C, L_001CFDBC)

This function takes an object pointer (in r2) and writes 8 consecutive floats from global constant tables to object+0xD0 through object+0xEC (offsets 208-236), while clearing numerous other fields.

```cpp
L_001CF618:                                    // unit_0033.cpp:4640
    c.r[3].sd[0] = static_cast<std::int32_t>(0x004E0000u);   // line 4641
    set32(c.r[3], c.r[3].w[0] + 14128u);                     // line 4642 — table @0x004E3790
    c.r[10].sd[0] = static_cast<std::int32_t>(0x00440000u);  // line 4643
    st32((c.r[2].w[0]), c.r[3].w[0]);                        // line 4644 — obj+0 = table ptr
    c.r[9].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4645
    c.r[3].sd[0] = static_cast<std::int32_t>(0x004E0000u);   // line 4646
    st32((c.r[2].w[0] + 8u), 0u);                            // line 4647 — obj+8 = 0
    set32(c.r[3], c.r[3].w[0] + 14608u);                     // line 4648 — table @0x004E3920
    st32((c.r[2].w[0] + 4u), 0u);                            // line 4649 — obj+4 = 0
    st32((c.r[2].w[0]), c.r[3].w[0]);                        // line 4650 — obj+0 = table ptr (overwrite)
    c.r[8].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4651
    st32((c.r[2].w[0] + 188u), 0u);                          // line 4652 — obj+0xBC = 0
    c.r[7].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4653
    st32((c.r[2].w[0] + 192u), 0u);                          // line 4654 — obj+0xC0 = 0
    c.r[6].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4655
    st32((c.r[2].w[0] + 196u), 0u);                          // line 4656 — obj+0xC4 = 0
    c.r[5].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4657
    st32((c.r[2].w[0] + 200u), 0u);                          // line 4658 — obj+0xC8 = 0
    c.r[4].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4659
    st32((c.r[2].w[0] + 204u), 0u);                          // line 4660 — obj+0xCC = 0
    c.r[3].sd[0] = static_cast<std::int32_t>(0x00440000u);   // line 4661
    c.f[3] = ld32((c.r[10].w[0] - 24720u));                  // line 4662 — float from table @0x00439F70
    c.f[2] = ld32((c.r[9].w[0] - 24716u));                   // line 4663 — float from table @0x00439F74
    c.f[1] = ld32((c.r[8].w[0] - 24712u));                   // line 4664 — float from table @0x00439F78
    c.f[0] = ld32((c.r[7].w[0] - 24708u));                   // line 4665 — float from table @0x00439F7C
    c.r[10].sd[0] = static_cast<std::int32_t>(0x004E0000u);  // line 4666
    set32(c.r[10], c.r[10].w[0] + 14560u);                   // line 4667 — table @0x004E38E0
    st32((c.r[2].w[0] + 208u), c.f[3]);                      // line 4668 — obj+0xD0 = float[0]
    st32((c.r[2].w[0] + 212u), c.f[2]);                      // line 4669 — obj+0xD4 = float[1]
    st32((c.r[2].w[0] + 216u), c.f[1]);                      // line 4670 — obj+0xD8 = float[2]
    st32((c.r[2].w[0] + 220u), c.f[0]);                      // line 4671 — obj+0xDC = float[3]
    c.f[3] = ld32((c.r[6].w[0] - 24672u));                   // line 4672 — float from table @0x004D9FA0
    c.f[2] = ld32((c.r[5].w[0] - 24668u));                   // line 4673 — float from table @0x004D9FA4
    c.f[1] = ld32((c.r[4].w[0] - 24664u));                   // line 4674 — float from table @0x004D9FA8
    c.f[0] = ld32((c.r[3].w[0] - 24660u));                   // line 4675 — float from table @0x004D9FAC
    st32((c.r[2].w[0] + 224u), c.f[3]);                      // line 4676 — obj+0xE0 = float[4]
    st32((c.r[2].w[0] + 228u), c.f[2]);                      // line 4677 — obj+0xE4 = float[5]
    st32((c.r[2].w[0] + 232u), c.f[1]);                      // line 4678 — obj+0xE8 = float[6]
    st32((c.r[2].w[0] + 236u), c.f[0]);                      // line 4679 — obj+0xEC = float[7]
    st32((c.r[2].w[0] + 180u), 0u);                          // line 4680 — obj+0xB4 = 0
    st32((c.r[2].w[0] + 184u), 0u);                          // line 4681 — obj+0xB8 = 0
    st32((c.r[2].w[0] + 24u), 0u);                           // line 4682 — obj+0x18 = 0
    st32((c.r[2].w[0] + 176u), 0u);                          // line 4683 — obj+0xB0 = 0
    ... return;
```

**3 variants:** L_001CF6D0 (line 4687), L_001CFA4C (line 4959), L_001CFDBC (line 5194) — identical structure, likely for different fighter types or orientation modes.

**Evidence:** Stores 8 consecutive floats to object offsets 0xD0-0xEC from global constant tables (0x00439F70-0x00439F7C and 0x004D9FA0-0x004D9FAC), consistent with an orientation/rotation setter writing rotation matrix or quaternion data. Unit_0033.cpp contains 15 vtable+0x4c call sites, confirming this slot is actively dispatched.

---

## 3. State configuration (+0x78) — NOT DEFINITIVELY IDENTIFIED

No single function body was isolated that clearly implements the state config vtable slot. Candidate patterns observed:

- **unit_0032.cpp:L_001CB640** (lines 4564-4576): Returns 0 or 1 based on comparing a value at object+7360 (0x1CB0) against 2 and 3. This is a state *query*, not a state *config* (which would accept a mode value and transition state).
- **unit_0032.cpp:L_001CB5E0** (lines 4537-4563): Fighter flags manipulation — reads fighter+0x520, ORs with bitmask, stores back. Also modifies fighter+0x28 (side controller). This is flag manipulation, not state transition.
- **unit_0033.cpp:L_001CE098** (lines 2997-3012): Writes fighter+0x538 = 56 (vtable index) and fighter+0x53c = 56 (mirror), plus clears numerous fields. This is part of post-placement init, not state config.

The state config slot (+0x78) would take a fighter pointer + mode/config value and transition the fighter's state. No function matching this signature (write an argument to a fighter state field) was definitively isolated.

---

## 4. Post-placement (+0xac) — LIKELY IMPLEMENTATION

**unit_0033.cpp at L_001CE070 (VA 0x001CE070), lines 2986-3019**

This function performs fighter post-placement initialization after spawn:

```cpp
L_001CE070:                                    // unit_0033.cpp:2986
    set32(c.r[17], c.r[17].w[0] + 1u);           // line 2987 — increment loop counter
    c.r[2].d[0] = c.r[17].sd[0] < 16ll ? 1ull : 0ull;  // line 2988 — check counter < 16
    set32(c.r[16], c.r[16].w[0] + 4u);           // line 2989 — advance pointer
    { if (t) goto L_001CE060; }                   // line 2991 — loop 16 times
    
    c.r[4].d[0] = c.r[20].d[0] + 0ull;           // line 2992 — r4 = fighter object
    c.r[5].d[0] = c.r[18].d[0] + 0ull;           // line 2993 — r5 = value
    st32((c.r[20].w[0] + 1316u), c.r[18].w[0]);  // line 2994 — fighter+0x524 = r18
    
    // L_001CE098 (line 2997) — continuation after sub-call:
    set32(c.r[8], ld32((c.r[28].w[0] - 3696u))); // line 2998 — load from global table
    set32(c.r[7], c.r[18].w[0] << 2);            // line 2999 — table index = r18 * 4
    set32(c.r[3], 0u + 56u);                     // line 3000 — r3 = 56 (0x38)
    set32(c.r[2], 0u - 1u);                      // line 3001 — r2 = -1
    set32(c.r[4], c.r[20].w[0] + 6304u);         // line 3002 — r4 = fighter+0x18B0
    c.r[5].d[0] = 0ull + 0ull;                   // line 3003
    set32(c.r[6], 0u + 32u);                     // line 3004
    set32(c.r[8], ld32((c.r[8].w[0] + 108u)));   // line 3005
    set32(c.r[7], c.r[7].w[0] + c.r[8].w[0]);    // line 3006
    set32(c.r[7], ld32((c.r[7].w[0] + 132u)));   // line 3007
    st32((c.r[20].w[0] + 6240u), c.r[7].w[0]);   // line 3008 — fighter+0x1860 = table value
    st32((c.r[20].w[0] + 6288u), c.r[2].w[0]);   // line 3009 — fighter+0x1890 = -1
    st32((c.r[20].w[0] + 1336u), c.r[3].w[0]);   // line 3010 — fighter+0x538 = 56 (vtable index)
    st32((c.r[20].w[0] + 1340u), c.r[3].w[0]);   // line 3011 — fighter+0x53c = 56 (vtable index mirror)
    st32((c.r[20].w[0] + 1344u), 0u);            // line 3012 — fighter+0x540 = 0
    st32((c.r[20].w[0] + 1812u), 0u);            // line 3013 — clear field
    st32((c.r[20].w[0] + 6048u), 0u);            // line 3014 — clear field
    st32((c.r[20].w[0] + 6228u), 0u);            // line 3015 — clear field
    st32((c.r[20].w[0] + 6232u), 0u);            // line 3016 — clear field
    st32((c.r[20].w[0] + 1324u), 0u);            // line 3017 — clear field
    st32((c.r[20].w[0] + 1328u), 0u);            // line 3018 — clear field
    ... return;
```

**Evidence:** This function:
1. Loops 16 times (initialization loop)
2. Writes fighter+0x524 (unknown field)
3. Sets fighter+0x538 = 56 (vtable table index) and fighter+0x53c = 56 (mirror)
4. Zeroes fighter+0x540 and numerous other fields
5. Calls sub-functions via unit_0034 and unit_0036

Unit_0033.cpp contains 1 vtable+0xac call site (confirmed in prior analysis). This function is the strongest candidate for the post-placement implementation.

---

## 5. Stage slot selector (+0x28) — NOT DEFINITIVELY IDENTIFIED

Units with game_stage field accesses (+0x08 stage_id, +0x24 variant_bit): unit_0002, unit_0007, unit_000C, unit_0011, unit_001C, unit_001E. All 6 units contain vtable+0x28 call patterns, but no single function body was isolated that:
- Takes a stage object pointer
- Reads stage+0x08 (stage_id) and/or stage+0x24 (variant_bit)
- Computes a slot value 0-4 (e.g., stage_id % 5 or variant-bit-based selection)
- Returns the result in r2

No block in these units both loads stage+0x08/stage+0x24 AND returns a small constant 0-4. The selector logic likely spans multiple basic blocks connected by jumps, making it difficult to isolate without full control-flow analysis.

---

## Summary table

| Struct | Slot | Offset | Implementation | Unit | Label | VA | Lines | Confidence |
|---|---|---|---|---|---|---|---|---|
| s_game_stage | slot selector | +0x28 (40) | Not found | — | — | — | — | ❌ Not identified |
| s_fighter_vtable | position setter | +0x40 (64) | Found | unit_0032.cpp | L_001CB540 | 0x001CB540 | 4500-4519 | ✅ Confirmed |
| s_fighter_vtable | orientation | +0x4c (76) | Found (4 variants) | unit_0033.cpp | L_001CF618 | 0x001CF618 | 4640-4685 | ⚠️ Likely |
| s_fighter_vtable | orientation | +0x4c (76) | Found (variant) | unit_0033.cpp | L_001CF6D0 | 0x001CF6D0 | 4687+ | ⚠️ Likely |
| s_fighter_vtable | orientation | +0x4c (76) | Found (variant) | unit_0033.cpp | L_001CFA4C | 0x001CFA4C | 4959+ | ⚠️ Likely |
| s_fighter_vtable | orientation | +0x4c (76) | Found (variant) | unit_0033.cpp | L_001CFDBC | 0x001CFDBC | 5194+ | ⚠️ Likely |
| s_fighter_vtable | state config | +0x78 (120) | Not found | — | — | — | — | ❌ Not identified |
| s_fighter_vtable | post-placement | +0xac (172) | Found | unit_0033.cpp | L_001CE070 | 0x001CE070 | 2986-3019 | ⚠️ Likely |

---

## Key observations

1. **Position setter writes to "spawn vec4" (+0x510)**, not a separate position field. The reconstruction's +0x510 field description as "spawn vec4" is accurate — it doubles as the active position being set by the vtable position setter.

2. **Orientation setter writes to offsets 0xD0-0xEC** (208-236), which are NOT documented in the reconstructed s_fighter layout. The reconstruction only documents fields at +0x510, +0x520, +0x534, +0x538, +0x53c. Offsets 0xD0-0xEC are within the fighter struct (sizeof 0x540) but were not documented, suggesting the reconstruction is incomplete.

3. **Post-placement init sets vtable index to 56** (0x38) at fighter+0x538/+0x53c, confirming the vtable index mechanism works as reconstructed.

4. **Four orientation setter variants** (L_001CF618, L_001CF6D0, L_001CFA4C, L_001CFDBC) suggest different fighter types or orientation modes share the same implementation pattern.
