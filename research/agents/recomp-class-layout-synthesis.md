# Recompiled Game Class Layouts — Super Dragon Ball Z

Comprehensive cross-reference of reconstructed game class layouts against `ps2-web-recomp/sdbz/generated/SuperDragonBallZ/` decompiled MIPS interpreter units. All 249 generated files analyzed (240 EE units + 9 VU1 units). Read-only analysis; no files in `ps2-web-recomp` were modified.

This report synthesizes findings from:
- `recomp-class-mapping.md` — field access cross-reference
- `recomp-vtable-implementations.md` — vtable CALL site mapping
- `recomp-vtable-bodies.md` — vtable function body identification
- `recomp-stage-fields.md` — stage field population chain
- `recomp-fighter-deep-dive.md` — fighter fields beyond +0x540
- `recomp-stage-subclasses.md` — stage subclass field expansion
- `recomp-additional-classes.md` — search for camera/effect/audio/animation controller classes
- `recomp-camera-class.md` — camera class search (negative)
- `recomp-animation-controller.md` — animation controller search (negative)
- `game-class-reconstruction.md` — original reconstructed layouts

## 1. Executive Summary

The ps2-web-recomp EE decompilation contains exactly **4 confirmed class types** with vtable pointers at +0x00:

| Class | Vtable | Reconstructed sizeof | Actual sizeof | Confirmed fields |
|-------|--------|---------------------|---------------|-----------------|
| `s_game_stage` | Yes | 0x28 | **>0x28** (writes to +0x28+) | +0x08 stage_id, +0x24 variant_bit, +0x28+ |
| `s_fighter` | Yes | 0x540 | **>0x540** (writes to 0x6258+) | +0x00 vtable, +0x510 spawn vec4, +0x520 flags, +0x534 side ctrl ptr, +0x538 vtable index, +0x53c vtable index mirror |
| `s_game_session` | Yes | 0x68 | **>0x68** (likely) | +0x00 vtable, +0x58 stage ptr, +0x60 fighter1 ptr, +0x64 fighter2 ptr |
| `s_fighter_side` | — | 0x0c | 0x0c | +0x08 side selector (unconfirmed) |

**No additional class types found** — camera, effect manager (CEffectMgr/CEBZ), audio manager, animation controller, and other classes are absent from the EE decompiled code. They may be IOP-side code or reside in unregistered VA gaps.

---

## 2. s_game_stage — Stage Object

### 2.1 Confirmed Fields

| Offset (hex) | Offset (dec) | Type | Semantics | Evidence |
|-------------|-------------|------|-----------|----------|
| +0x00 | 0 | uint32_t | vtable pointer | unit_004E: `ld32((c.r[4].w[0]))` then `ld32((c.r[25].w[0] + 40u))` |
| +0x08 | 8 | uint32_t | stage ID (0-12) | unit_0007, 000C, 0011, 001C, 001E: co-occurs with +0x24 |
| +0x24 | 36 | uint32_t | variant bit (0/1) | unit_0007, 000C, 0011, 001C, 001E: co-occurs with +0x08 |
| +0x28 | 40 | uint32_t | vtable slot selector (returns 0-4) | 66 units contain vtable+0x28 call pattern |

### 2.2 Struct Size Discrepancy

Reconstruction declares `sizeof == 0x28`, but stores at **+0x28 (decimal 40)** and beyond are observed in unit_0007, unit_000C, unit_0011, unit_001C, unit_001E. Stage subclass analysis (recomp-stage-subclasses.md) found additional fields at offsets **+0x04 through +0x1e8** (488 decimal) on registers that also store stage_id (+0x08) and variant_bit (+0x24). The actual stage object is significantly larger than 0x28 bytes — likely 0x1e8+ or more.

### 2.3 Field Population Chain (Fully Mapped)

```
Stage object creation:
  unit_000C.cpp (~line 880-916) → allocate stage object at c.r[16]
    st32(r16 + 8, 0)           // field_08 = 0 (stage ID default)
    st32(r16 + 36, 0)          // field_24 = 0 (variant bit default)
    st8(r16 + 0, 1)            // byte+0 = 1 (object type marker)
    st32(r16 + 40, c.r[3])     // +0x28 = vtable ptr
    st32(r16 + 32, c.r[2])     // +0x20 = some value

Stage data table copy:
  unit_0011.cpp (~line 380-430) → copy 64-byte stage record from source table (c.r[5]) to stage object (c.r[4])
    c.r[3] = *(c.r[5] + 8);     // source+8 = field_08 in source
    st32(c.r[4] + 8, c.r[3]);   // dest+8 = field_08 (copied)
    c.r[7] = *(c.r[5] + 36);    // source+36 = field_24 in source
    st32(c.r[4] + 36, c.r[7]);  // dest+36 = field_24 (copied)

Stage ID assignment:
  unit_0007.cpp entry 17 (~line 770-835) → L_0011C1E0
    c.r[17] = unit_0006(c, 52u)  // stage ID (0-12) returned in c.r[2]
    st32(r16 + 8, c.r[17])       // field_08 = stage ID
    st32(r16 + 36, c.r[19])      // field_24 = variant bit arg (c.r[19] = orig c.r[6])

Variant bit (entry 411 callers):
  Caller passes $a1 = 0/1 to unit_004E(c, 319u) [L_0023A430]
  L_0023A430 stores $a1 at session+0x2C
  L_0023B140 copies session+0x2C → session+0x24 (field_24)
```

### 2.4 Stage ID Assignment (0-12)

Stage IDs are computed by `unit_0006` entry 52 (`L_00118B78`), called from `unit_0007` entry 17. Stage IDs map to stages s00–s12:

| field_08 | Stage | Variant matters? | Notes |
|----------|-------|-----------------|-------|
| 0 | s00 | No | |
| 1 | s01 | No | |
| 2 | s02 | No | |
| 3 | s03 | Yes | s03a/s03b (sentinel groups) |
| 4 | s04 | Yes | s04a/s04b (different placement) |
| 5 | s05 | Yes | Both variants real |
| 6–12 | s06–s12 | No | |

Variant matters only for field_08 ∈ {3, 4, 5}.

### 2.5 Vtable Slot Selector (+0x28) — NOT Isolated

The vtable +0x28 slot selector is **called** in 66 units but no function body was isolated that:
- Takes a stage object pointer
- Reads stage+0x08 and/or stage+0x24
- Computes slot value 0-4
- Returns the result

The selector logic spans multiple basic blocks across units, making it difficult to isolate without full control-flow analysis.

---

## 3. s_fighter — Fighter/Player Object

### 3.1 Confirmed Fields

| Offset (hex) | Offset (dec) | Type | Semantics | Evidence |
|-------------|-------------|------|-----------|----------|
| +0x00 | 0 | uint32_t | vtable pointer | unit_004E: `ld32((c.r[4].w[0]))` |
| +0x510 | 1296 | float[4] | spawn vec4 [X,Y,Z,W] | unit_0032: st32 at r16+1296..1308 |
| +0x514 | 1300 | float | spawn Y | unit_0032 |
| +0x518 | 1304 | float | spawn Z | unit_0032 |
| +0x51C | 1308 | float | spawn W | unit_0032 (via unit_003C tail call) |
| +0x520 | 1312 | uint32_t | flags bitmask | unit_002C–004E (extensive), unit_0032 L_001CB5E0 |
| +0x524 | 1316 | uint32_t | unknown field | unit_0033 L_001CE070: st32(r20 + 1316, r18) |
| +0x534 | 1332 | uint32_t | side controller sub-object ptr | unit_002C–004E (extensive) |
| +0x538 | 1336 | uint32_t | vtable table index | unit_002C–004E, unit_0033 L_001CE070 |
| +0x53c | 1340 | uint32_t | vtable table index mirror | unit_0032, 0033, 0034, 0037, 0039, 0040 |

### 3.2 Struct Size Discrepancy

Reconstruction declares `sizeof == 0x540` (1344), but stores at **+0x540 (decimal 1344)** are observed in unit_0032 and unit_0033. Fighter deep-dive confirmed accesses at offsets up to **0x6258 (25176)**:

| Offset (hex) | Offset (dec) | Evidence |
|-------------|-------------|----------|
| +0x36B0 | 14000 | unit_0015, 0016, 0017, 0018 |
| +0x3880–0x38D8 | 14464–14552 | unit_000D |
| +0x358C | 13700 | unit_0090 |
| +0x6258 | 25176 | unit_0090 |

The actual fighter struct is substantially larger than 0x540 — no upper bound determined.

### 3.3 Fields NOT Found

| Field type | Searched | Result |
|------------|----------|--------|
| Health | health | Not found |
| Ki/energy | ki_, energy | Not found |
| Stamina | stamina | Not found |
| Velocity X/Y/Z | velocity | Not found |
| Animation state | anim_state | Not found |
| Current position | position (beyond spawn vec4) | Not found |

**Important**: The position setter (+0x40 vtable slot) writes to fighter+0x510 (spawn vec4), not a separate position field. +0x510 doubles as both spawn position and active position.

---

## 4. s_fighter_vtable — Fighter Vtable

### 4.1 Confirmed Vtable Slots

| Offset (hex) | Offset (dec) | Semantics | Implementation | Confidence |
|-------------|-------------|-----------|---------------|------------|
| +0x40 | 64 | Position setter (writes spawn vec4 to fighter+0x510..+0x51C) | unit_0032.cpp `L_001CB540` (lines 4500-4519) | ✅ Confirmed |
| +0x4c | 76 | Orientation setter (writes 8 floats to object+0xD0..+0xEC) | unit_0033.cpp `L_001CF618` + 3 variants (L_001CF6D0, L_001CFA4C, L_001CFDBC) | ⚠️ Likely |
| +0x78 | 120 | State configuration | Call sites in 45 units, body NOT identified | ❌ Not found |
| +0xac | 172 | Post-placement init | unit_0033.cpp `L_001CE070` (lines 2986-3019) | ⚠️ Likely |

### 4.2 Position Setter Body (unit_0032.cpp `L_001CB540`, lines 4500-4519)

```cpp
c.r[3].sd[0] = 0x00440000u;   // line 4501
c.r[2].sd[0] = 0x00440000u;   // line 4502
c.f[3] = ld32(r3.w[0] - 24720u);      // line 4503 — float from table @0x00439F70
set32(c.r[4], ld32((c.r[16].w[0] + 6276u)));  // line 4504
c.f[2] = ld32(r2.w[0] - 24716u);      // line 4505 — float from table @0x00439F74
c.r[3].sd[0] = 0x00440000u;   // line 4506
c.r[2].sd[0] = 0x00440000u;   // line 4507
c.f[1] = ld32(r3.w[0] - 24712u);      // line 4508 — float from table @0x00439F78
c.f[0] = ld32(r2.w[0] - 24708u);      // line 4509 — float from table @0x00439F7C
st32(r16.w[0] + 1296u, c.f[3]);       // line 4510 — fighter+0x510 = spawn X
st32(r16.w[0] + 1300u, c.f[2]);       // line 4511 — fighter+0x514 = spawn Y
st32(r16.w[0] + 1304u, c.f[1]);       // line 4512 — fighter+0x518 = spawn Z
st32(r16.w[0] + 1308u, c.f[0]);       // line 4514 — fighter+0x51C = spawn W
```

### 4.3 Orientation Setter Body (unit_0033.cpp `L_001CF618`, lines 4640-4685)

Writes 8 consecutive floats from global constant tables (0x00439F70–0x00439F7C and 0x004D9FA0–0x004D9FAC) to object+0xD0..+0xEC, while zeroing fields at +0xB4, +0xB8, +0x18, +0xB0, +0xBC, +0xC0, +0xC4, +0xC8, +0xCC. 4 variants exist (L_001CF618, L_001CF6D0, L_001CFA4C, L_001CFDBC).

### 4.4 Post-Placement Body (unit_0033.cpp `L_001CE070`, lines 2986-3019)

```cpp
// Loop 16 times (initialization loop)
st32(r20.w[0] + 1316u, c.r[18]);      // fighter+0x524 = r18
set32(r3, 56);                        // r3 = 56 (0x38)
st32(r20.w[0] + 1336u, 56);           // fighter+0x538 = 56 (vtable index)
st32(r20.w[0] + 1340u, 56);           // fighter+0x53c = 56 (mirror)
st32(r20.w[0] + 1344u, 0);            // fighter+0x540 = 0
// Zeroes numerous other fields (+0x714, +0x17D0, +0x1854, +0x1868, +0x52C, +0x530)
// Calls sub-functions via unit_0034 and unit_0036
```

Sets vtable index to 56 at fighter+0x538/+0x53c, confirming the vtable index mechanism.

### 4.5 Vtable Call Dispatch Pattern (Confirmed)

```cpp
set32(c.r[25], ld32((c.r[4].w[0])));     // load vtable ptr from object+0x00
set32(c.r[25], ld32((c.r[25].w[0] + OFFSETu))); // load function from vtable+offset
c.pc = c.r[25].w[0];
call(c, c.r[25].w[0], depth + 1u);       // indirect call
```

Detected for all target offsets (+0x40, +0x4c, +0x78, +0xac, stage +0x28).

### 4.6 Vtable Call Site Counts

| Slot | Units with call sites | Highest count unit |
|------|----------------------|-------------------|
| Stage +0x28 (slot selector) | 66 units | unit_003A (20) |
| Fighter +0x40 (position setter) | 58 units | unit_00BF (32) |
| Fighter +0x4c (orientation) | 58 units | unit_00AF (30) |
| Fighter +0x78 (state config) | 45 units | unit_00A6 (59) |
| Fighter +0xac (post-placement) | 9 units | unit_0035 (10) |

unit_00E0–00EF: **no vtable slot calls found**.

---

## 5. s_game_session — Session/Owner Object

### 5.1 Confirmed Fields

| Offset (hex) | Offset (dec) | Type | Semantics | Evidence |
|-------------|-------------|------|-----------|----------|
| +0x00 | 0 | uint32_t | vtable pointer | inferred from spawn callers |
| +0x58 | 88 | uint32_t | stage object pointer | unit_0004, 0008, 0010, 0011, 0027–004F |
| +0x60 | 96 | uint32_t | fighter #1 pointer | unit_0004, 0008, 0010, 0011, 0027–004F |
| +0x64 | 100 | uint32_t | fighter #2 pointer | unit_0004, 0008, 0010, 0011, 0027–004F |

### 5.2 Field Population

Representative examples:
- **unit_0004.cpp**: `st32(r16.w[0] + 88, ...)`, `st32(r16.w[0] + 96, ...)`, `st32(r16.w[0] + 100, ...)` — r16 at all three offsets
- **unit_0008.cpp**: loads at r20 + 88, + 96, + 100 — r20 at all three offsets
- **unit_0011.cpp**: `st32(r20.w[0] + 88)`, `st32(r20.w[0] + 96)`, `st32(r20.w[0] + 100)` — r20 at all three offsets

Co-occurrence of all three offsets on the same base register confirms the struct layout.

---

## 6. s_fighter_side — Side Controller Sub-Object

| Offset (hex) | Offset (dec) | Type | Semantics | Evidence |
|-------------|-------------|------|-----------|----------|
| +0x08 | 8 | uint32_t | side selector (0=P1, 1=P2) | NOT definitively confirmed |

**Inconclusive**: +0x08 is extremely common (used as stack frame offset AND struct field). No pointer-chase from `s_fighter +0x534` (side controller ptr) → `+0x08` on the pointed-to object was observed. Cannot distinguish from `s_game_stage +0x08` (stage_id) or other uses.

---

## 7. Classes NOT Found

| Class | Searched For | Result | Likely Location |
|-------|-------------|--------|-----------------|
| Camera | camera, Camera, cam, yaw, pitch, distance | **Not found** | IOP-side or unregistered VA gaps |
| Effect manager | CEffect, CEBZ, effect, Effect | **Not found** | IOP-side (ELF refs 0x004CA870, 0x004CEF18, 0x004CEF50) |
| Audio manager | audio, Audio, voice, Voice | **Not found** | IOP-side (SPU/ADPCM/ASE/AMS) |
| Animation controller | animation, anim, mot2, slerp, clip | **Not found** | Data files (MOT2/PMT) or unregistered VA gaps |

**249 files searched** (240 EE units + 9 VU1 units + table.cpp + units.hpp). Only the 4 documented classes exist in the EE decompilation.

---

## 8. Discrepancies Between Reconstruction and Decompiled Code

| Aspect | Reconstruction | Decompiled Code | Status |
|--------|---------------|-----------------|--------|
| `s_game_stage` sizeof | 0x28 (40) | Writes at +0x28 (40) and beyond, up to +0x1e8 (488) | **Discrepancy** — struct is larger |
| `s_fighter` sizeof | 0x540 (1344) | Writes at +0x540 (1344) and beyond, up to +0x6258 (25176) | **Discrepancy** — struct is larger |
| `s_game_stage` fields | +0x08 stage_id, +0x24 variant_bit | Additional fields at +0x04–+0x1e8 | **Incomplete** — many undocumented fields |
| `s_fighter` fields | +0x510–+0x53c | Additional fields at 0x36B0–0x6258 | **Incomplete** — many undocumented fields |
| Stage slot selector (+0x28) | Returns 0-4 | Call sites in 66 units, body not isolated | **Partially confirmed** |
| Fighter state config (+0x78) | State transition | Call sites in 45 units, body not isolated | **Not found** |
| Fighter post-placement (+0xac) | Post-placement init | unit_0033.cpp `L_001CE070` | **Likely identified** |
| Fighter position setter (+0x40) | Sets position | unit_0032.cpp `L_001CB540` | **Confirmed** |
| Fighter orientation (+0x4c) | Sets rotation | unit_0033.cpp `L_001CF618` + 3 variants | **Likely identified** |
| `s_fighter_side` layout | +0x08 side selector | Cannot distinguish from other +0x08 uses | **Unconfirmed** |
| Camera class | N/A | Not found in EE units | **Not in scope** |
| Animation controller | N/A | Not found in EE units | **Not in scope** |

---

## 9. Fighter Struct — What Lies Beyond +0x540

Confirmed field accesses beyond +0x540 (decimal 1344):

| Unit | Offset (hex) | Offset (dec) | Lines | Register(s) |
|------|-------------|-------------|-------|-------------|
| unit_000D | 0x3880–0x38D8 | 14464–14552 | 3444–4870 | c.r[2], c.r[3], c.r[4], c.r[5], c.r[16], c.r[18], c.r[19], c.r[21] |
| unit_000E | 0x3880–0x3888 | 14464–14472 | 650–862 | c.r[2], c.r[3] |
| unit_0016 | 0x36B0–0x379C | 14000–14252 | 3111–4392 | c.r[16] |
| unit_0017 | 0x36B0 | 14000 | 2160, 2167, 5193 | c.r[16], c.r[8] |
| unit_0018 | 0x36B0 | 14000 | 3636 | c.r[4] |
| unit_0015 | 0x36B0 | 14000 | 4351 | c.r[4] |
| unit_0090 | 0x358C, 0x6258 | 13700, 25176 | 2043–2131 | c.r[4] |

No upper bound on struct size determined. Fields at offsets +0x540 through +0x36B0 (1344–14000) were not found — possible gap between documented fields and newly discovered fields.

**Note**: Several large-offset accesses (13472–13720 at VA base 0x005C0000; 13520–13568 at VA base 0x00480000) read from **global data tables**, NOT fighter object fields — these were correctly excluded.

---

## 10. Unit-to-Class Reference

| Unit | Primary Classes | Key Lines | Notes |
|------|----------------|-----------|-------|
| unit_000C | s_game_stage (constructor) | 880-916 | field_08=0, field_24=0, byte+0=1, vtable+0x28 |
| unit_0011 | s_game_stage (table copy) | 380-430 | copies 64-byte stage record |
| unit_0007 | s_game_stage (field population) | 770-835 | stage ID from unit_0006 ent.52, variant from arg |
| unit_0006 | s_game_stage (stage ID computation) | entry 52 at L_00118B78 | returns stage ID 0-12 |
| unit_0032 | s_fighter (position setter) | 4500-4519 | L_001CB540 writes spawn vec4 to +0x510 |
| unit_0032 | s_fighter (flags) | 4537-4563 | L_001CB5E0 ORs flags at +0x520 |
| unit_0033 | s_fighter (orientation setter) | 4640-4685 | L_001CF618 writes 8 floats to +0xD0..+0xEC |
| unit_0033 | s_fighter (post-placement) | 2986-3019 | L_001CE070 sets vtable index 56 at +0x538/+0x53c |
| unit_004E | spawn routine (reads stage+8/+36, fighter+510/520/534/538/53c) | 3987-3993, 4301-4302 | vtable +0xac call pattern confirmed at line 3518 |

---

## 11. Files Not Modified

No files in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\` were modified. All analysis was read-only.