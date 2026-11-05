# Recompiled stage class subclasses — Super Dragon Ball Z

Search for stage subclass implementations in `ps2-web-recomp/sdbz/generated/SuperDragonBallZ/`
generated MIPS interpreter units. Read-only analysis; no files in `ps2-web-recomp` were modified.

## Source references

- `research/agents/game-class-reconstruction.md` — reconstructed s_game_stage layout (+0x08 stage_id, +0x24 variant_bit, sizeof 0x28, vtable +0x28 slot selector)
- `research/agents/recomp-class-mapping.md` — confirmed stage field accesses in units 0002, 0007, 000C, 0011, 001C, 001E
- `research/agents/recomp-vtable-implementations.md` — vtable +0x28 slot selector call sites in 66 units

## Search scope

All generated units (`unit_0000.cpp` through `unit_00DF.cpp`, plus `unit_00E0`–`unit_00EF`).
Total units: ~240.

## Key finding: s_game_stage has additional fields beyond +0x24

The reconstruction declares `sizeof(s_game_stage) == 0x28`, with only two fields:
- +0x08 (8): stage_id
- +0x24 (36): variant_bit

However, field access analysis across all units reveals that objects storing both stage_id (+0x08) and variant_bit (+0x24) on the same base register ALSO write to numerous additional offsets. These are stage subclass fields that extend beyond the reconstructed 0x28-byte size.

## Stage initialization — units with stage_id (+0x08) or variant_bit (+0x24) stores

**201 units** contain stores to stage_id (+0x08) and/or variant_bit (+0x24). Units in the prior class-mapping range (0000–004F) that were already confirmed: unit_0002, unit_0007, unit_000C, unit_0011, unit_001C, unit_001E.

### Stage object construction (same register stores both +0x08 and +0x24)

Many units have base registers that receive stores at BOTH +0x08 (stage_id) and +0x24 (variant_bit), confirming these are stage objects. Example register patterns:

**unit_0002.cpp** — stage_id stores:
```
st32((c.r[4].w[0] + 8u), c.f[1]);          — r4+0x08 = stage_id
st32((c.r[20].w[0] + 8u), c.r[3].w[0]);     — r20+0x08 = stage_id
st32((c.r[16].w[0] + 8u), c.r[5].w[0]);     — r16+0x08 = stage_id
st32((c.r[3].w[0] + 8u), c.f[0]);           — r3+0x08 = stage_id
```

```
st32((c.r[4].w[0] + 36u), c.f[3]);          — r4+0x24 = variant_bit
st32((c.r[16].w[0] + 36u), c.r[5].w[0]);    — r16+0x24 = variant_bit
st32((c.r[17].w[0] + 36u), c.f[0]);         — r17+0x24 = variant_bit
```

Registers r4, r16, r17 appear with both +0x08 and +0x24 stores, confirming they are stage object pointers. However, stage_id and variant_bit stores are typically in **separate basic blocks** (connected by jumps), not a single initialization function. Stage construction is distributed across multiple blocks within a unit.

## Stage subclass fields beyond +0x24

For registers that store both +0x08 and +0x24, additional store offsets were found. These represent stage subclass fields. The most commonly occurring additional offsets across many units:

| Offset (hex) | Offset (dec) | Frequency | Likely purpose |
|---|---|---|---|
| +0x04 | 4 | Very common | Stage parameter / pointer |
| +0x0c | 12 | Very common | Stage parameter |
| +0x10 | 16 | Very common | Stage parameter |
| +0x14 | 20 | Very common | Stage parameter |
| +0x18 | 24 | Very common | Stage parameter |
| +0x1c | 28 | Very common | Stage parameter |
| +0x20 | 32 | Very common | Stage parameter |
| +0x28 | 40 | Common | Field immediately after variant_bit |
| +0x2c | 44 | Common | Field after +0x28 |
| +0x30 | 48 | Common | — |
| +0x34 | 52 | Common | — |
| +0x38 | 56 | Common | — |
| +0x3c | 60 | Common | — |
| +0x40 | 64 | Common | — |
| +0x44 | 68 | Common | — |
| +0x48 | 72 | Common | — |
| +0x4c | 76 | Common | — |
| +0x50 | 80 | Common | — |
| +0x54 | 84 | Common | — |
| +0x58 | 88 | Common | — |
| +0x5c | 92 | Common | — |
| +0x60 | 96 | Common | — |
| +0x64 | 100 | Common | — |
| +0x68 | 104 | Common | — |
| +0x6c | 108 | Common | — |
| +0x70 | 112 | Very common | — |
| +0x74 | 116 | Very common | — |
| +0x78 | 120 | Very common | — |
| +0x7c | 124 | Very common | — |
| +0x80 | 128 | Common | — |
| +0x84 | 132 | Common | — |
| +0x88 | 136 | Common | — |
| +0x8c | 140 | Common | — |
| +0x90 | 144 | Common | — |
| +0x94 | 148 | Common | — |
| +0x98 | 152 | Common | — |
| +0x9c | 156 | Common | — |
| +0xa0 | 160 | Common | — |
| +0xa4 | 164 | Common | — |
| +0xa8 | 168 | Common | — |
| +0xac | 172 | Common | — |
| +0xb0 | 176 | Common | — |
| +0xb4 | 180 | Common | — |
| +0xb8 | 184 | Common | — |
| +0xbc | 188 | Common | — |
| +0xc0 | 192 | Common | — |
| +0xc4 | 196 | Common | — |
| +0xc8 | 200 | Common | — |
| +0xcc | 204 | Common | — |
| +0xd0 | 208 | Common | — |
| +0xd4 | 212 | Common | — |
| +0xd8 | 216 | Common | — |
| +0xdc | 220 | Common | — |
| +0xe0 | 224 | Common | — |
| +0xe4 | 228 | Common | — |
| +0xe8 | 232 | Common | — |
| +0xec | 236 | Common | — |
| +0xf0 | 240 | Common | — |
| +0xf4 | 244 | Common | — |
| +0xf8 | 248 | Common | — |
| +0xfc | 252 | Common | — |
| +0x100 | 256 | Common | — |
| +0x104 | 260 | Common | — |
| +0x108 | 264 | Common | — |
| +0x10c | 268 | Common | — |
| +0x110 | 272 | Common | — |
| +0x114 | 276 | Common | — |
| +0x118 | 280 | Common | — |
| +0x11c | 284 | Common | — |
| +0x120 | 288 | Common | — |
| +0x124 | 292 | Common | — |
| +0x128 | 296 | Common | — |
| +0x12c | 300 | Common | — |

### Example: unit_000C.cpp stage object

Register r16 stores both stage_id (+0x08) and variant_bit (+0x24), with additional fields:
```
st32((c.r[16].w[0] + 8u), ...);    — stage_id
st32((c.r[16].w[0] + 36u), ...);   — variant_bit
st32((c.r[16].w[0] + 40u), ...);   — +0x28 (immediately after variant_bit)
st32((c.r[16].w[0] + 36u), 0u);    — variant_bit = 0 (another write)
```

### Example: unit_009E.cpp stage object (4 stage registers)

```
st32((c.r[16].w[0] + 8u), ...);     — stage_id
st32((c.r[16].w[0] + 36u), ...);    — variant_bit
Additional fields on r16: +0x0c, +0x10, +0x14, +0x20, +0x30, +0x34, +0x38, +0x70, +0x74, +0x78, +0x7c, +0xac
```

### Example: unit_009F.cpp stage object (16 stage registers!)

```
st32((c.r[16].w[0] + 8u), ...);     — stage_id
st32((c.r[16].w[0] + 36u), ...);    — variant_bit
Additional fields on r16: +0x04, +0x0c, +0x10, +0x14, +0x18, +0x20, +0x28, +0x2c, +0x30, +0x34, +0x38, +0x40, +0x44, +0x48, +0x4c, +0x50, +0x54, +0x60, +0x64, +0x68, +0x70, +0x74, +0xa0, +0xa4, +0xa8, +0xac, +0xb0, +0xb4, +0xb8, +0xbc, +0xc0, +0xc4, +0xd0, +0xd4, +0xd8, +0xdc, +0xe0, +0xe4, +0xe8, +0xec, +0xf0, +0x140, +0x144, +0x148, +0x14c, +0x1d0, +0x1d4, +0x1d8, +0x1dc, +0x1e0, +0x1e4, +0x1e8
```

The presence of offsets up to +0x1e8 (488) on some stage objects indicates that stage subclasses can be significantly larger than the reconstructed 0x28 bytes.

## vtable slot selector (+0x28) — call sites confirmed, implementations not isolated

The vtable +0x28 slot selector is **called** in 66 units (confirmed in recomp-vtable-implementations.md), but no separate function body was identified that:
- Takes a stage object pointer
- Reads stage+0x08 (stage_id) and/or stage+0x24 (variant_bit)
- Computes a slot value 0-4
- Returns the result

The slot selector logic likely spans multiple basic blocks across units, making it difficult to isolate as a single function. Unit_000C.cpp had 11 stage_id loads and 11 variant_bit loads (the highest co-occurrence), suggesting it contains significant stage logic, but no single block returns a small constant 0-4 after loading stage data.

## Different vtable layouts per stage — NOT confirmed

No evidence was found of different vtable slot layouts per stage subclass. All vtable +0x28 call patterns follow the same dispatch sequence:
1. Load vtable pointer from object+0x00
2. Load function from vtable+0x28 (slot selector offset)
3. Indirect call

The +0x28 offset is consistently used across all 66 units, suggesting a single vtable layout for all stage subclasses, rather than per-subclass vtable layouts.

## Stage types (s00-s12) — not identifiable

The reconstruction references stage types s00-s12, but no stage-type-specific field values or type identifiers were found in the decompiled units. Stage IDs are stored as integers at +0x08, but no mapping from stage ID to stage type name (s00-s12) is visible in the generated code.

## Discrepancies between reconstruction and decompiled code

| Aspect | Reconstruction | Decompiled code | Status |
|---|---|---|---|
| s_game_stage sizeof | 0x28 (40 bytes) | Writes at +0x28 (40) and beyond observed | **Discrepancy** — struct is larger |
| Stage fields | +0x08 stage_id, +0x24 variant_bit | Additional fields at +0x04–+0x1e8 observed | **Incomplete** — many undocumented fields |
| vtable slot selector (+0x28) | Returns 0-4 per stage | Call sites in 66 units, implementation not isolated | **Partially confirmed** — calls exist, body not found |
| Stage subclass vtables | Unknown | Single +0x28 offset used across all units | **Consistent** — no per-subclass layout variation found |
| Stage types s00-s12 | Referenced in reconstruction | Not identifiable in decompiled code | **Not found** |

## Summary

- **201 units** contain stage_id (+0x08) and/or variant_bit (+0x24) stores
- **Stage objects confirmed** in units where same register stores both +0x08 and +0x24 (e.g., unit_0002, unit_000C, unit_009E, unit_009F, unit_00A2, unit_00A4, unit_00AA, unit_00AB, unit_00AC, unit_00AD, unit_00AF, unit_00B1, unit_00B2, unit_00B3, unit_00B4, unit_00B6, unit_00B7, unit_00C8, unit_00C9)
- **Additional stage subclass fields** found at offsets +0x04 through +0x1e8 (488), far beyond the reconstructed sizeof 0x28
- **Most common additional fields**: +0x04, +0x0c, +0x10, +0x14, +0x18, +0x1c, +0x20 (preceding variant_bit), +0x28 through +0x12c (following variant_bit)
- **No per-stage vtable layout variation** found — all units use +0x28 consistently
- **No stage-type-specific implementations** (s00-s12) identifiable
- **Slot selector implementation** not isolated — call sites exist but function body spans multiple blocks
