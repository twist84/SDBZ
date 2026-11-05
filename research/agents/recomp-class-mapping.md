# Recompiled class mapping — Super Dragon Ball Z

Cross-reference of reconstructed game class layouts against `ps2-web-recomp/sdbz/generated/SuperDragonBallZ/`
decompiled MIPS interpreter units. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Source references

- `research/agents/game-class-reconstruction.md` — reconstructed layouts (s_game_stage, s_fighter, s_fighter_vtable, s_game_session, s_fighter_side)
- `research/agents/character-animation-systems.md` — ELF research confirming fighter offsets were NOT found as direct immediates in SLES_541.61 (section 13.1)

## Search scope

Generated units checked:
- `unit_0000.cpp` through `unit_004F.cpp` (0x00–0x4F)
- `unit_00E0.cpp` through `unit_00EF.cpp` (0xE0–0xEF)

Field accesses appear in the MIPS interpreter as `ld32((c.r[N].w[0] + OFFSETu))` / `st32((c.r[N].w[0] + OFFSETu), ...)`. Decimal offsets used:

| Reconstructed | Decimal | Meaning |
|---|---|---|
| s_game_stage +0x08 | 8 | stage ID |
| s_game_stage +0x24 | 36 | variant bit |
| s_game_stage vtable +0x28 | 40 | slot selector |
| s_fighter +0x510 | 1296 | spawn vec4 |
| s_fighter +0x520 | 1312 | flags |
| s_fighter +0x534 | 1332 | side controller ptr |
| s_fighter +0x538 | 1336 | vtable index |
| s_fighter +0x53c | 1340 | vtable index mirror |
| s_fighter +0x540 | 1344 | sizeof boundary |
| s_fighter_vtable +0x40 | 64 | position setter |
| s_fighter_vtable +0x4c | 76 | orientation |
| s_fighter_vtable +0x78 | 120 | state config |
| s_fighter_vtable +0xac | 172 | post-placement |
| s_game_session +0x58 | 88 | stage |
| s_game_session +0x60 | 96 | fighter1 |
| s_game_session +0x64 | 100 | fighter2 |
| s_fighter_side +0x08 | 8 | side selector |

---

## 1. s_fighter (+0x510 / +0x520 / +0x534 / +0x538 / +0x53c) — CONFIRMED

Fighter offset accesses are concentrated in units `unit_002C` through `unit_004E`. All six fighter offsets (0x510–0x540) co-occur on the same base registers (r4, r5, r16–r20), confirming the struct layout.

| Offset | Decimal | Evidence | Units |
|---|---|---|---|
| +0x510 spawn vec4 | 1296 | float4 loads/stores via `c.f[3]`, `c.f[0]`, `ld128` | 0032, 0033, 0034, 0036, 0043 |
| +0x520 flags | 1312 | uint32 loads/stores | 002C–004E (extensive) |
| +0x534 side ctrl ptr | 1332 | uint32 pointer loads/stores | 002C–004E (extensive) |
| +0x538 vtable index | 1336 | uint32 loads | 002C–004E (extensive) |
| +0x53c vtable index mirror | 1340 | uint32 loads/stores | 0032, 0033, 0034, 0037, 0039, 0040 |
| +0x540 sizeof boundary | 1344 | stores at/ past sizeof | 0032, 0033 |

**Units with fighter field accesses:**
unit_002C, unit_0031, unit_0032, unit_0033, unit_0034, unit_0036, unit_0037, unit_0038, unit_0039, unit_003B, unit_003C, unit_003D, unit_003E, unit_003F, unit_0040, unit_0041, unit_0042, unit_0043, unit_0044, unit_0045, unit_0046, unit_0047, unit_0048, unit_0049, unit_004A, unit_004B, unit_004C, unit_004D, unit_004E

**Units without fighter field accesses:** unit_0000–002B, unit_004F, unit_00E0–00EF

---

## 2. s_game_session (+0x58 / +0x60 / +0x64) — CONFIRMED

Game session field accesses (+88/+96/+100) co-occur on the same base registers across many units, confirming stage/fighter1/fighter2 layout.

Representative examples:
- `unit_0004.cpp`: `st32((c.r[16].w[0] + 88u), ...)`, `st32((c.r[16].w[0] + 96u), ...)`, `st32((c.r[16].w[0] + 100u), ...)` — same base r16 at +0x58/+0x60/+0x64
- `unit_0008.cpp`: `c.r[2].d[0] = ld8((c.r[20].w[0] + 88u)); set32(c.r[2], ld32((c.r[20].w[0] + 96u))); ld16((c.r[20].w[0] + 100u))` — r20 at all three offsets
- `unit_0010.cpp`: `st32((c.r[20].w[0] + 88u), ...)`, `st32((c.r[20].w[0] + 96u), ...)`, `st32((c.r[20].w[0] + 100u), ...)` — r20 at all three offsets
- `unit_0011.cpp`: `sdr(c.r[6], (c.r[2].w[0] + 88u)); sdr(c.r[7], (c.r[2].w[0] + 96u));` — double-word loads at +0x58/+0x60

**Units with co-occurring game_session offsets:**
unit_0004, unit_0005, unit_0006, unit_0008, unit_0009, unit_0010, unit_0011, unit_0013, unit_0014, unit_0018, unit_0027, unit_0028, unit_0029, unit_002B, unit_0030, unit_0031, unit_0032, unit_0033, unit_0034, unit_0035, unit_0036, unit_0037, unit_0038, unit_003A, unit_003B, unit_003C, unit_003D, unit_003E, unit_003F, unit_0040, unit_0041, unit_0042, unit_0043, unit_0044, unit_0045, unit_0046, unit_0047, unit_0048, unit_0049, unit_004A, unit_004B, unit_004C, unit_004D, unit_004E, unit_004F

---

## 3. s_game_stage (+0x08 / +0x24) — PARTIALLY CONFIRMED

Game stage stage_id (+0x08) and variant_bit (+0x24) co-occur on the same base register in several units:

- `unit_0007.cpp`: `ld32((c.r[16].w[0] + 8u)); st32((c.r[16].w[0] + 36u), ...); st32((c.r[16].w[0] + 8u), ...);` — r16 at +0x08 and +0x24
- `unit_000C.cpp`: `st32((c.r[16].w[0] + 8u), 0u); st32((c.r[16].w[0] + 36u), 0u); st32((c.r[16].w[0] + 40u), ...)` — r16 at +0x08, +0x24, +0x28
- `unit_0011.cpp`: `st32((c.r[4].w[0] + 8u), ...); st32((c.r[4].w[0] + 36u), ...); st32((c.r[4].w[0] + 40u), ...)` — r4 at +0x08, +0x24, +0x28
- `unit_001C.cpp`: `ld32((c.r[16].w[0] + 36u)); ld32((c.r[16].w[0] + 8u));` — r16 at +0x24 and +0x08
- `unit_001E.cpp`: `st32((c.r[16].w[0] + 8u), ...); st32((c.r[16].w[0] + 36u), 0u);` — r16 at +0x08 and +0x24

**Units with co-occurring game_stage offsets:** unit_0002, unit_0007, unit_000C, unit_0011, unit_001C, unit_001E

---

## 4. s_fighter_side (+0x08) — INCONCLUSIVE

+0x08 is extremely common across all units (used both as stack frame offset and struct field). It co-occurs with game_stage offsets (+0x24) on the same base registers, so it cannot be definitively attributed to `s_fighter_side::m_side_selector` versus `s_game_stage::m_stage_id` or other uses. No side-controller-pointer dereference chain (`+0x534` → `+0x08`) was clearly observed.

---

## 5. s_fighter_vtable (+0x40 / +0x4c / +0x78 / +0xac) — PARTIALLY CONFIRMED

| Offset | Decimal | Evidence | Units |
|---|---|---|---|
| +0x40 position setter | 64 | uint32 loads | unit_0028 (r4, r21), unit_001D (r6 as immediate) |
| +0x4c orientation | 76 | uint32 loads/stores | unit_0027 (r25, r4, r2), unit_001D (r3 as immediate) |
| +0x78 state config | 120 | Only as immediate value (`set32(c.r[3], 0u + 120u)`) | unit_001D — no field access observed |
| +0xac post-placement | 172 | **Vtable indirect call pattern**: `ld32((c.r[4].w[0]))` loads vtable ptr from object, then `ld32((c.r[25].w[0] + 172u))` loads the slot, followed by indirect `call(c, t, ...)` | unit_004E (line ~3518–3521) |

The +0xac post-placement slot is the only vtable offset confirmed via the classic virtual-call pattern (load vtable pointer from object+0x00, then load function slot at vtable+offset, indirect call).

---

## 6. unit_00E0–00EF — NO TARGET ACCESSES

None of the E0–EF units contain any target offset field accesses on non-stack (non-r29) registers. All small-offset matches in these units are stack frame accesses (`c.r[29].w[0] + OFFSET`).

---

## Discrepancies between reconstructed layouts and decompiled code

### D1: s_game_stage sizeof — writes at +0x28 (offset 40)
The reconstruction declares `sizeof == 0x28`, but field accesses at +0x28 (decimal 40) are observed (e.g. `unit_0007.cpp`, `unit_000C.cpp`, `unit_0011.cpp`). Offset 0x28 is exactly at the struct boundary, meaning either:
- `s_game_stage` is larger than 0x28 bytes, or
- +0x28 accesses a field immediately following the struct (e.g. an adjacent object or array element).

### D2: s_fighter sizeof — writes at +0x540 (offset 1344)
The reconstruction declares `sizeof == 0x540`, but stores at +0x540 are observed (`unit_0032.cpp`, `unit_0033.cpp`). Offset 0x540 is exactly at the struct boundary, suggesting the fighter struct may be larger than 0x540 bytes, or +0x540 is the start of a subsequent field/struct.

### D3: s_fighter_vtable +0x78 state config — not found as field access
Offset +0x78 (decimal 120) appears only as an immediate value (`set32(c.r[3], 0u + 120u)` in `unit_001D.cpp`), never as `ld32((base.w[0] + 120u))` / `st32((base.w[0] + 120u), ...)` on an object/vtable pointer. This slot is unverified in the decompiled code.

### D4: s_fighter_side layout — not independently confirmed
`+0x08` (side_selector) cannot be distinguished from `s_game_stage +0x08` (stage_id) or other uses. No pointer-chase from `s_fighter +0x534` (side_controller_ptr) to `+0x08` on the pointed-to object was clearly observed.

### D5: Fighter struct may be larger than 0x540
Combined with D2, writes at +0x540 and the co-occurrence of all fighter fields (0x510–0x540) suggest the actual fighter object may extend beyond 0x540 bytes. Additional fields (health, ki, stamina, move/frame data, animation state) noted as missing in `game-class-reconstruction.md` section 9.3 may occupy space past 0x540.

---

## Summary table

| Struct | Offset | Status | Units |
|---|---|---|---|
| s_game_stage | +0x08 stage_id | Confirmed (co-occurs with +0x24) | 0002, 0007, 000C, 0011, 001C, 001E |
| s_game_stage | +0x24 variant_bit | Confirmed (co-occurs with +0x08) | 0002, 0007, 000C, 0011, 001C, 001E |
| s_game_stage | sizeof 0x28 | **Discrepancy** (access at +0x28) | — |
| s_game_stage vtable | +0x28 slot selector | Not directly verified as vtable slot | — |
| s_fighter | +0x510 spawn vec4 | Confirmed (float4 access) | 0032, 0033, 0034, 0036, 0043 |
| s_fighter | +0x520 flags | Confirmed | 002C–004E |
| s_fighter | +0x534 side ctrl ptr | Confirmed | 002C–004E |
| s_fighter | +0x538 vtable index | Confirmed | 002C–004E |
| s_fighter | +0x53c vtable index mirror | Confirmed | 0032, 0033, 0034, 0037, 0039, 0040 |
| s_fighter | sizeof 0x540 | **Discrepancy** (access at +0x540) | 0032, 0033 |
| s_fighter_vtable | +0x40 position setter | Confirmed (direct access) | 0028, 001D |
| s_fighter_vtable | +0x4c orientation | Confirmed (direct access) | 0027, 001D |
| s_fighter_vtable | +0x78 state config | Not found as field access | — |
| s_fighter_vtable | +0xac post-placement | Confirmed (vtable indirect call) | 004E |
| s_game_session | +0x58 stage | Confirmed | 0004–004F (many) |
| s_game_session | +0x60 fighter1 | Confirmed | 0004–004F (many) |
| s_game_session | +0x64 fighter2 | Confirmed | 0004–004F (many) |
| s_fighter_side | +0x08 side_selector | Inconclusive (+0x08 too common) | — |
| All | unit_00E0–00EF | No target accesses | — |
