# Recompiled Spawn Routine — Stage Field Population Analysis

## 1. Overview

This document traces how `field_08` (stage ID, +0x08) and `field_24` (variant bit, +0x24) get POPULATED in the stage object before the spawn routine (`unit_004E.cpp`) reads them. The spawn routine only READS these fields — it never writes them. Population happens in stage object creation/initialization code in `unit_0007`, `unit_000C`, and `unit_0011`, and via caller-provided variant bits for entry 411 callers.

All analysis is read-only against `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`. No files modified.

## 2. Stage Object Creation — unit_000C (field_08=0, field_24=0 defaults)

**`unit_000C.cpp`** (around line 880) initializes a new stage object at `c.r[16]`:

```
st32((c.r[16].w[0] + 8u), 0u);     // field_08 = 0  (stage ID default)
...
st32((c.r[16].w[0] + 36u), 0u);    // field_24 = 0  (variant bit default)
st8((c.r[16].w[0]), static_cast<std::uint8_t>(1u));  // byte+0 = 1 (object type/vtable marker)
st32((c.r[16].w[0] + 40u), c.r[3].w[0]);  // +0x28 = vtable ptr (from c.r[3] = 0x00130000 + 320)
st32((c.r[16].w[0] + 32u), c.r[2].w[0]);  // +0x20 = some value
```

This is the stage object constructor: zero-initializes field_08 and field_24, sets the object type byte at +0, and installs the vtable at +0x28. After this, `unit_0023(c, 142u)` (memory allocator) is called, then stage data is copied in from a table (see §3).

## 3. Stage Data Table Copy — unit_0011 (field_08 from source+8, field_24 from source+36)

**`unit_0011.cpp`** entry at `L_001440E8` (line 380) copies a 64-byte stage data record from a source table (`c.r[5]`) to the stage object (`c.r[4]`):

```
c.r[8] = *(c.r[5] + 0);   c.r[5] += 4   // source+0
c.r[7] = *(c.r[5] + 0);   c.r[5] += 4   // source+4
c.r[3] = *(c.r[5] + 0);   c.r[5] += 4   // source+8   <-- field_08 in source
c.r[2] = *(c.r[5] + 0);   c.r[5] += 4   // source+12
st32(c.r[4] + 0, c.r[8]);   // dest+0
st32(c.r[4] + 4, c.r[7]);   // dest+4
st32(c.r[4] + 8, c.r[3]);   // dest+8 = source+8  (field_08 copied)
st32(c.r[4] + 12, c.r[2]);  // dest+12
...
c.r[7] = *(c.r[5] + 0);   c.r[5] += 4   // source+36  <-- field_24 in source
...
st32(c.r[4] + 36, c.r[7]);   // dest+36 = source+36 (field_24 copied)
```

This preserves the stage table's field_08 (at source+8) and field_24 (at source+36) into the stage object. The stage data table itself contains entries with stage ID at +8 and variant bit at +36.

## 4. Stage ID Assignment — unit_0007 entry 17 (field_08 = stage ID 0-12)

**`unit_0007.cpp`** entry 17 (`L_0011C1E0`, line 770) is the stage field population function:

```
c.r[16] = c.r[4];            // save stage object base
c.r[19] = c.r[6];            // save variant bit arg (becomes field_24 source)
c.r[20] = c.r[5];            // save another arg
c.r[8] = c.r[6];             // variant bit passed to unit_0006
...call unit_0006(c, 131u)... // variant-related processing
...call unit_0006(c, 99u)...  // stage table lookup at data addr
...call unit_0006(c, 52u)...  // returns stage ID (0-12) in c.r[2]
c.r[17] = c.r[2];            // stage ID
st32((c.r[16].w[0] + 36u), c.r[19].w[0]);   // field_24 = variant bit (c.r[19] = orig c.r[6])
st32((c.r[16].w[0] + 8u), c.r[17].w[0]);    // field_08 = stage ID (c.r[17] = unit_0006 entry 52 result)
st32((c.r[16].w[0] + 40u), c.r[18].w[0]);   // +0x28 = other field
```

**unit_0007 entry 17** is called with:
- `$a0` = `c.r[4]` = stage object base
- `$a1` = `c.r[5]` = additional argument (saved to c.r[20])
- `$a2` = `c.r[6]` = variant bit (0/1, saved to c.r[19], becomes field_24)

**`unit_0006` entry 52** (`L_00118B78`) computes/returns the stage ID (0–12). This is where stage IDs 0–12 are assigned — likely from a stage table indexed by the current stage selection or runtime state. The exact table is at data addresses referenced by `unit_0006(c, 99u)` calls (e.g., `0x004BCD50`, `0x004BCD78`).

## 5. Variant Bit Determination — Two Paths

### 5.1 Entry 399 callers (stage object already populated)

For entry 399 callers (`L_0023AC90`), the stage object passed as `$a0` already has field_08 and field_24 set from stage initialization (unit_0007 entry 17). The spawn routine reads them directly:
- `stage+8` → field_08 (stage ID 0-12)
- `stage+36` → field_24 (variant bit 0/1)

No caller modifies these fields before entry 399.

### 5.2 Entry 411 callers (variant passed by caller)

For entry 411 callers (`L_0023B140`), the variant bit is passed by the caller via `L_0023A430` (entry 319):

**Caller sets `$a1 = variant_bit` (0 or 1), calls `unit_004E(c, 319u)`:**

| Caller | Line | `$a1` value | Resulting field_24 |
|--------|------|-------------|-------------------|
| unit_0050.cpp | 2491 | `0` | variant 0 |
| unit_0054.cpp | 2734 | `0` | variant 0 |
| unit_0055.cpp | 2863 | `1` | variant 1 |
| unit_0055.cpp | 3421 | `0` | variant 0 |
| unit_005F.cpp | 3893 | `1` | variant 1 |
| Others | — | `0` | variant 0 |

`L_0023A430` (`unit_004E.cpp:3367`) stores `$a1` at `session+0x2C`. Then `L_0023B140` (`unit_004E.cpp:4301-4302`) copies `session+0x2C` to `session+0x24`:
```
st32((c.r[20].w[0] + 44u), c.r[5].w[0]);   // *(session + 0x2C) = $a1 (variant bit)
...
st32((c.r[20].w[0] + 36u), c.r[5].w[0]);   // *(session + 0x24) = *(session + 0x2C) = field_24
```

**`unit_0055.cpp`** calls `L_0023A430` twice — once with `$a1=1` (line 2863) and once with `$a1=0` (line 3421) — setting up different field_24 variants for two fighters.

## 6. Stage IDs 0-12 — Assignment Mechanism

Stage IDs 0–12 (13 stages) are assigned by `unit_0006` entry 52, called from `unit_0007` entry 17. The function references stage data table addresses (e.g., `0x004BCD50`, `0x004BCD78` via `unit_0006(c, 99u)`). Stage IDs map to stage folders `s00`–`s12`:

| field_08 | Stage | Variant matters? | Notes |
|----------|-------|-----------------|-------|
| 0 | s00 | No | Both variants → same group |
| 1 | s01 | No | |
| 2 | s02 | No | |
| 3 | s03 | Yes | s03a/s03b (groups 6/7, both sentinel) |
| 4 | s04 | Yes | s04a/s04b (different placement configs) |
| 5 | s05 | Yes | Both variants real (g7/g8) |
| 6–12 | s06–s12 | No | Both variants → same group |

Variant matters only for field_08 ∈ {3, 4, 5}. Entries 24–27 in the map fall back to group 0 (sentinel).

## 7. Summary of Field Population Chain

```
Stage object creation:
  unit_000C.cpp:880-916  →  alloc stage obj, field_08=0, field_24=0, byte+0=1, vtable+0x28

Stage data from table:
  unit_0011.cpp:380-430  →  copy 64-byte stage record, field_08 (src+8→dst+8), field_24 (src+36→dst+36)

Stage ID assignment:
  unit_0007.cpp:770-835  →  entry 17, calls unit_0006 entry 52 for stage ID (0-12),
                           sets field_08 = stage ID, field_24 = variant bit arg

Variant bit (entry 411 callers):
  Caller passes $a1 = 0/1 to unit_004E(c, 319u) [L_0023A430]
  L_0023A430 stores at session+0x2C
  L_0023B140 copies session+0x2C → session+0x24 (field_24)

Spawn routine read (entry 399):
  unit_004E.cpp:3987-3988  →  reads stage+8 (field_08) and stage+36 (field_24)
                           →  map_index = field_08*2 + field_24
```

## 8. Units Involved

| Unit | Role | Key Lines |
|------|------|-----------|
| unit_000C.cpp | Stage object constructor (defaults field_08=0, field_24=0) | 880-916 |
| unit_0011.cpp | Stage data table copy (preserves field_08/+8, field_24/+36) | 380-430 |
| unit_0007.cpp | Stage field population (field_08 from unit_0006 ent.52, field_24 from arg) | 770-835 (entry 17) |
| unit_0006.cpp | Stage ID computation (entry 52), variant processing (entry 131), stage table lookup (entry 99) | 131 dispatch, L_00118B78 |
| unit_004E.cpp | Spawn routine — reads field_08/+8, field_24/+36 (entry 399); field_24 setup via session+0x24 (entry 411) | 3987-3993, 4301-4302 |
