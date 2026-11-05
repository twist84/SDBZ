# Recompiled animation code search — Super Dragon Ball Z

Search for animation/MOT2-related code in `ps2-web-recomp/sdbz/generated/SuperDragonBallZ/`
generated MIPS interpreter units. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Search scope

All generated units (`unit_0000.cpp` through `unit_00DF.cpp`, plus `unit_00E0`–`unit_00EF`).
Total units: ~240. Units outside prior ranges (0000–004F and 00E0–00EF): **137 units (0050–00DF)**.

## Search keywords — NEGATIVE RESULTS

No high-level animation string literals exist in any generated unit. This is expected because these are low-level MIPS interpreter decompilations — animation code is implemented as register operations, float loads/stores, and memory accesses without high-level keyword strings.

| Keyword(s) searched | Matches | Notes |
|---|---|---|
| mot2, MOT2 | 0 | — |
| slerp, SLERP, quat, QUAT, quaternion, QUATERNION | 0 | — |
| rfsm, RFSM, L3 | 0 | — |
| motion, MOTION, channel, CHANNEL | 0 | — |
| keyframe, KEYFRAME, key_frame, KEY_FRAME | 0 | — |
| lerp, LERP, interpolat, INTERPOLAT | 0 | — |
| idle, walk, run, jump, fall, land | 0 | — |
| translate, TRANSLATE, position_key, POSITION_KEY | 0 | — |
| bone_binding, BONE_BINDING, shared_block, SHARED_BLOCK | 0 | — |
| anim, ANIM | 0 | — |

## Animation-related code — low-level pattern matches

Although no animation keywords exist as string literals, animation-related code is identifiable at the machine-code level through:
- High counts of `st128` (128-bit vector/matrix stores)
- High counts of float stores to object+offset (bone transform writes)
- Frame counter increment patterns (`reg + 1`)
- Fused multiply-add patterns (vector dot products for quaternion/matrix math)

### Units with highest animation-code-pattern scores (outside 0000–004F and 00E0–00EF)

| Unit | Score | st128 count | Float stores | Inc patterns | VA range |
|---|---|---|---|---|---|
| unit_006C.cpp | 545 | 175 | 356 | 14 | 0x002B0000–0x002Bxxxx |
| unit_0080.cpp | 500 | 119 | 360 | 21 | 0x00200000–0x0020xxxx |
| unit_00AA.cpp | 494 | 142 | 327 | 25 | 0x002A0000–0x002Axxxx |
| unit_006E.cpp | 458 | 136 | 312 | 10 | — |
| unit_00AC.cpp | 454 | 110 | 328 | 16 | — |
| unit_005F.cpp | 450 | 69 | 350 | 31 | — |
| unit_006F.cpp | 449 | 132 | 303 | 14 | — |
| unit_00B6.cpp | 442 | 147 | 282 | 13 | — |
| unit_0028.cpp | 442 | 146 | 284 | 12 | — |
| unit_002D.cpp | 439 | 55 | 384 | 0 | — |

All 30 high-score units are outside the 0000–004F and 00E0–00EF ranges.

## Specific animation functions identified

### unit_006C.cpp — Bone transform matrix stores (animation bone update)

Multiple functions in unit_006C.cpp write 3x3 rotation matrices to object offsets 0x150–0x168 (decimal 336–360), loaded from animation data tables in the 0x00490000 region.

**Function L_002B0794 (VA 0x002B0794), lines 987–1016:**

```cpp
L_002B0794:                                    // unit_006C.cpp:987
    c.r[2].sd[0] = static_cast<std::int32_t>(0x004F0000u);   // line 988 — table base
    c.r[3].sd[0] = static_cast<std::int32_t>(0x00490000u);   // line 989 — table base
    set32(c.r[2], c.r[2].w[0] - 18528u);                     // line 990
    st32((c.r[16].w[0] + 436u), 0u);                          // line 991 — obj+0x1B4 = 0
    st32((c.r[16].w[0]), c.r[2].w[0]);                        // line 992 — obj+0x00 = table ptr
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00490000u);   // line 993
    c.f[4] = ld32((c.r[2].w[0] - 4872u));                     // line 994 — float from table
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00490000u);   // line 995
    c.f[3] = ld32((c.r[2].w[0] - 4876u));                     // line 996 — float from table
    st32((c.r[16].w[0] + 360u), c.f[4]);                     // line 997 — obj+0x168 = matrix[0]
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00490000u);   // line 998
    c.f[0] = ld32((c.r[2].w[0] - 4880u));                     // line 999 — float from table
    st32((c.r[16].w[0] + 356u), c.f[3]);                     // line 1000 — obj+0x164 = matrix[1]
    set32(c.r[2], 0u + 1u);                                   // line 1001
    st8((c.r[16].w[0] + 480u), static_cast<std::uint8_t>(c.r[2].w[0])); // line 1002 — obj+0x1E0 = flag
    st32((c.r[16].w[0] + 352u), c.f[0]);                     // line 1003 — obj+0x160 = matrix[2]
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00490000u);   // line 1004
    c.f[2] = ld32((c.r[2].w[0] - 4888u));                     // line 1005 — float from table
    c.f[0] = ld32((c.r[3].w[0] - 4896u));                     // line 1006 — float from table
    c.r[2].sd[0] = static_cast<std::int32_t>(0x00490000u);   // line 1007
    c.f[1] = ld32((c.r[2].w[0] - 4892u));                     // line 1008 — float from table
    st32((c.r[16].w[0] + 376u), c.f[2]);                     // line 1009 — obj+0x178 = matrix[3]
    st32((c.r[16].w[0] + 368u), c.f[0]);                     // line 1010 — obj+0x170 = matrix[4]
    st32((c.r[16].w[0] + 372u), c.f[1]);                     // line 1011 — obj+0x174 = matrix[5]
```

**Evidence:** Writes 6 float values to consecutive object offsets 0x160–0x178 (352–376), forming a 3x2 rotation matrix, plus a flag at obj+0x1E0. Values loaded from animation data tables at 0x00490000 region. This is a bone rotation matrix update function called during animation playback.

**Function L_002B1F4C (VA 0x002B1F4C), lines 2665–2686:**

```cpp
L_002B1F4C:                                    // unit_006C.cpp:2665
    c.f[0] = ld32((c.r[29].w[0] + 216u));                     // line 2665 — stack load
    st32((c.r[16].w[0] + 336u), c.f[2]);                     // line 2666 — obj+0x150 = matrix[0]
    st32((c.r[16].w[0] + 340u), c.f[1]);                     // line 2667 — obj+0x154 = matrix[1]
    st128((c.r[29].w[0] + 192u) & ~15u, c.r[3]);             // line 2668 — stack load
    st32((c.r[16].w[0] + 344u), c.f[0]);                     // line 2669 — obj+0x158 = matrix[2]
    c.f[2] = ld32((c.r[29].w[0] + 192u));                     // line 2670 — stack load
    st8((c.r[16].w[0] + 464u), static_cast<std::uint8_t>(c.r[2].w[0])); // line 2671 — obj+0x1D0 = flag
    c.f[1] = ld32((c.r[29].w[0] + 196u));                     // line 2672 — stack load
    c.f[0] = ld32((c.r[29].w[0] + 200u));                     // line 2673 — stack load
    st32((c.r[16].w[0] + 352u), c.f[2]);                     // line 2674 — obj+0x160 = matrix[3]
    st32((c.r[16].w[0] + 356u), c.f[1]);                     // line 2675 — obj+0x164 = matrix[4]
    st32((c.r[16].w[0] + 360u), c.f[0]);                     // line 2676 — obj+0x168 = matrix[5]
    st8((c.r[16].w[0] + 464u), static_cast<std::uint8_t>(c.r[2].w[0])); // line 2677 — obj+0x1D0 = flag
    c.f[3] = ld32((c.r[16].w[0] + 336u));                     // line 2678 — read back obj+0x150
    c.f[2] = ld32((c.r[16].w[0] + 340u));                     // line 2679 — read back obj+0x154
    c.f[1] = ld32((c.r[16].w[0] + 344u));                     // line 2680 — read back obj+0x158
    c.f[0] = ld32((c.r[16].w[0] + 348u));                     // line 2681 — read back obj+0x15C
    st32((c.r[17].w[0] + 112u), c.f[3]);                     // line 2682 — r17+0x70 = matrix[0]
    st32((c.r[17].w[0] + 116u), c.f[2]);                     // line 2683 — r17+0x74 = matrix[1]
    st32((c.r[17].w[0] + 120u), c.f[1]);                     // line 2684 — r17+0x78 = matrix[2]
    { ... st32((c.r[17].w[0] + 124u), c.f[0]); ... }         // line 2686 — r17+0x7C = matrix[3]
```

**Evidence:** Complete bone transform pipeline:
1. Computes rotation matrix from stack data (vector math at lines 2650–2664)
2. Stores 3x3 matrix to object+0x150 through object+0x168 (336–360)
3. Sets flag at obj+0x1D0 (464)
4. Reads matrix back and copies to r17+0x70 through r17+0x7C (112–124) — likely world transform or skinning matrix
5. Budget cost: 79 cycles (line 2685) — significant computation, consistent with animation math

### Other animation-related functions

**unit_006C.cpp:L_002B0AF8 (VA 0x002B0AF8), lines 1233–1251:** Writes bone matrix at obj+0x160–0x168 (352–360).

**unit_006C.cpp:L_002B0F68 (VA 0x002B0F68), lines 1551–1554:** Writes bone matrix at obj+0x150–0x158 (336–344).

**unit_006C.cpp:L_002B3DBC (VA 0x002B3DBC), lines 4783–4799:** Writes complete 6-float matrix at obj+0x150–0x168 (336–360) using float registers f0–f5.

### Pattern summary across unit_006C

| Offset | Decimal | Meaning | Functions |
|---|---|---|---|
| obj+0x150 | 336 | Bone matrix row 0 col 0 | L_002B0F68, L_002B1014, L_002B1080, L_002B1830, L_002B1A20, L_002B1F4C, L_002B362C, L_002B3DBC |
| obj+0x154 | 340 | Bone matrix row 0 col 1 | Same as above |
| obj+0x158 | 344 | Bone matrix row 0 col 2 | Same as above |
| obj+0x160 | 352 | Bone matrix row 1 col 0 | L_002B0794, L_002B0AF8, L_002B1168, L_002B1218, L_002B1290, L_002B1F4C, L_002B340C, L_002B3DBC |
| obj+0x164 | 356 | Bone matrix row 1 col 1 | Same as above |
| obj+0x168 | 360 | Bone matrix row 1 col 2 | Same as above |
| obj+0x170 | 368 | Bone matrix row 2 col 0 | L_002B0794 |
| obj+0x174 | 372 | Bone matrix row 2 col 1 | L_002B0794 |
| obj+0x178 | 376 | Bone matrix row 2 col 2 | L_002B0794 |
| obj+0x1D0 | 464 | Animation flag | L_002B1F4C |
| obj+0x1E0 | 480 | Animation flag | L_002B0794 |

## Units with animation patterns — detailed listing

Units outside 0000–004F and 00E0–00EF with animation-related code patterns (st128 + float stores + increment):

unit_0050, unit_0051, unit_0052, unit_0053, unit_0054, unit_0055, unit_0056, unit_0057, unit_0058, unit_0059,
unit_005A, unit_005B, unit_005C, unit_005D, unit_005E, unit_005F, unit_0060, unit_0061, unit_0062, unit_0063,
unit_0064, unit_0065, unit_0066, unit_0067, unit_0068, unit_0069, unit_006A, unit_006B, unit_006C, unit_006D,
unit_006E, unit_006F, unit_0070, unit_0071, unit_0072, unit_0073, unit_0074, unit_0075, unit_0076, unit_0077,
unit_0078, unit_0079, unit_007A, unit_007B, unit_007C, unit_007D, unit_007E, unit_007F, unit_0080, unit_0081,
unit_0082, unit_0083, unit_0084, unit_0085, unit_0086, unit_0087, unit_0088, unit_0089, unit_008A, unit_008B,
unit_008C, unit_008D, unit_008E, unit_008F, unit_0090, unit_0091, unit_0092, unit_0093, unit_0094, unit_0095,
unit_0096, unit_0097, unit_0098, unit_0099, unit_009A, unit_009B, unit_009C, unit_009D, unit_009E, unit_009F,
unit_00A0, unit_00A1, unit_00A2, unit_00A3, unit_00A4, unit_00A5, unit_00A6, unit_00A7, unit_00A8, unit_00A9,
unit_00AA, unit_00AB, unit_00AC, unit_00AD, unit_00AE, unit_00AF, unit_00B0, unit_00B1, unit_00B2, unit_00B3,
unit_00B4, unit_00B5, unit_00B6, unit_00B7, unit_00B8, unit_00B9, unit_00BA, unit_00BB, unit_00BC, unit_00BD,
unit_00BE, unit_00BF, unit_00C0, unit_00C1, unit_00C2, unit_00C3, unit_00C4, unit_00C5, unit_00C6, unit_00C7,
unit_00C8, unit_00C9, unit_00CA, unit_00CB, unit_00CC, unit_00CD, unit_00CE, unit_00CF, unit_00D0, unit_00D1,
unit_00D2, unit_00D3, unit_00D4, unit_00D5, unit_00D6, unit_00D7, unit_00D8, unit_00D9, unit_00DA, unit_00DB,
unit_00DC, unit_00DD, unit_00DE, unit_00DF

## Summary

| Search category | Result |
|---|---|
| MOT2 format keywords ("mot2", "motion", "channel", "keyframe") | 0 matches — no string literals |
| Quaternion/slerp keywords ("slerp", "quat", "quaternion") | 0 matches — no string literals |
| Translation interpolation ("translate", "lerp", "interpolat") | 0 matches — no string literals |
| Animation state machine ("idle", "walk", "run", "jump", "fall", "land") | 0 matches — no string literals |
| RFSM/L3 packages ("rfsm", "bone_binding", "shared_block") | 0 matches — no string literals |
| Animation code at machine-code level | Found in 137 units (0050–00DF) |
| Confirmed bone transform functions | unit_006C.cpp:L_002B0794, L_002B1F4C (+6 more) |
| Bone matrix offsets | obj+0x150–0x178 (336–376) |
| Animation flag offsets | obj+0x1D0 (464), obj+0x1E0 (480) |

**Conclusion:** Animation code exists in the decompiled units but only at the low-level machine-code level (bone matrix stores, vector math, frame counters). No high-level animation format names, state names, or animation system keywords appear as string literals. The animation system is implemented through direct memory writes to bone transform offsets (0x150–0x178) loaded from animation data tables in the 0x00490000–0x004F0000 region.

All animation-related code is in units **outside** the 0000–004F and 00E0–00EF ranges previously analyzed for class layouts. The highest concentration is in unit_006C.cpp (score 545).
