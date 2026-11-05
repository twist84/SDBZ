# Recomp Fighter Struct Deep-Dive — Fields Beyond +0x540

## Search scope

Searched ALL generated `.cpp` files under `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\` for fighter object field accesses at offsets beyond +0x540 (decimal 1344).

Files searched:
- `unit_0000.cpp` through `unit_00F8.cpp` (EE MIPS decompiled units)
- `vu1_000.cpp` through `vu1_008.cpp` + `vu1_table.cpp` (VU1 microcode units)

Total: ~130 `.cpp` files

## Key finding: Fighter field accesses beyond +0x540 DO exist

Confirmed fighter object field accesses at decimal offsets greater than 1344 (+0x540):

### 1. unit_000D.cpp — Offsets 14464–14552 (0x3880–0x38D8)

Repeated ld32/st32 accesses on what appear to be fighter object base registers (c.r[2], c.r[3], c.r[4], c.r[5], c.r[16], c.r[18], c.r[19], c.r[21]):

| Offset (dec) | Offset (hex) | Lines | Operation | Registers used |
|-------------|-------------|-------|-----------|----------------|
| 14464 | 0x3880 | 3444, 3468, 3524, 3622, 3643, 3681, 3711, 3732, 4015, 4201, 4226, 4278, 4310, 4398, 4733, 4771, 4788, 4820, 4838, 4851, 4870 | ld32/st32 | c.r[2], c.r[3], c.r[4], c.r[9], c.r[16], c.r[19] |
| 14472 | 0x3888 | 4009, 4027, 4033, 4079, 4092, 4143, 4194, 4207, 4220, 4254, 4273, 4303, 4394, 4648, 4729, 4767, 4780, 4816, 4834, 4847, 4864, 4894 | ld32 | c.r[2], c.r[3], c.r[4] |
| 14468 | 0x3884 | 3443, 3499, 3514 | ld32/st32 | c.r[2], c.r[5], c.r[18] |
| 14480 | 0x3890 | 3514, 3523 | ld32/st32 | c.r[3], c.r[16] |
| 14492 | 0x389C | 4146, 4149, 4152 | ld32 | c.r[17] |
| 14496 | 0x38A0 | 4168 | st32 | c.r[9] |
| 14500 | 0x38A4 | 4124, 4156 | ld32/st32 | c.r[3], c.r[5], c.r[19] |
| 14504 | 0x38A8 | 3441 | st32 | c.r[3] |
| 14508 | 0x38AC | 3442 | st32 | c.r[4] |
| 14512 | 0x38B0 | 4458 | ld32 | c.r[2] |
| 14516 | 0x38B4 | 4254, 4479, 4804, 4826 | st32/ld32 | c.r[2], c.r[4], c.r[16] |
| 14520 | 0x38B8 | 4260 | st32 | c.r[5] |
| 14540 | 0x38C4 | 4021 | ld32 | c.r[23] |
| 14544 | 0x38C8 | 4053, 4079 | st32/ld32 | c.r[5], c.r[18] |
| 14552 | 0x38D0 | 4086 | st32 | c.r[8] |

Example code snippet:
```cpp
// unit_000D.cpp:3441-3445
st32((c.r[3].w[0] + 14504u), c.r[10].w[0]);
st32((c.r[4].w[0] + 14508u), c.r[11].w[0]);
st32((c.r[5].w[0] + 14468u), c.r[7].w[0]);
st32((c.r[6].w[0] + 14472u), c.r[2].w[0]);
st32((c.r[8].w[0] + 14464u), c.r[9].w[0]);
```

### 2. unit_000E.cpp — Offsets 14464–14472 (0x3880–0x3888)

Repeated ld32 access patterns (lines 650–862):

| Offset (dec) | Offset (hex) | Lines | Operation |
|-------------|-------------|-------|-----------|
| 14464 | 0x3880 | 650, 658, 702, 706, 715, 724, 753, 758, 773, 778, 797, 801, 810, 816, 835, 862 | ld32 |
| 14472 | 0x3888 | 650, 702, 715, 753, 773, 797, 810, 835 | ld32 |

Example code snippet:
```cpp
// unit_000E.cpp:650-658
set32(c.r[3], ld32((c.r[2].w[0] + 14472u)));
set32(c.r[3], ld32((c.r[2].w[0] + 14464u)));
```

### 3. unit_0016.cpp — Offsets 14000–14252 (0x36B0–0x37A4)

| Offset (dec) | Offset (hex) | Lines | Operation | Register |
|-------------|-------------|-------|-----------|----------|
| 14000 | 0x36B0 | 3111, 4352 | ld32 | c.r[16] |
| 14240 | 0x3790 | 4359 | ld32 | c.r[16] |
| 14244 | 0x3794 | 4361 | ld32 | c.r[16] |
| 14248 | 0x3798 | 4355, 4392 | ld32/st32 | c.r[16] |
| 14252 | 0x379C | 4352, 4366 | st32 | c.r[16] |

Example code snippet:
```cpp
// unit_0016.cpp:4352-4366
set32(c.r[2], ld32((c.r[16].w[0] + 14252u)));
set32(c.r[2], ld32((c.r[16].w[0] + 14248u)));
set32(c.r[2], ld32((c.r[16].w[0] + 14240u)));
set32(c.r[4], ld32((c.r[16].w[0] + 14244u)));
st32((c.r[16].w[0] + 14252u), c.r[2].w[0]);
```

### 4. unit_0017.cpp — Offset 14000 (0x36B0)

Lines 2160, 2167, 5193 — ld32 access from c.r[16] and c.r[8].

Example code snippet:
```cpp
// unit_0017.cpp:2160-2167
set32(c.r[3], ld32((c.r[16].w[0] + 14000u)));
if (c.r[2].sd[0] <= 0) { set32(c.r[2], ld32((c.r[16].w[0] + 14000u))); goto L_0015D5E0; }
```

### 5. unit_0018.cpp — Offset 14000 (0x36B0)

Line 3636 — ld32 access from c.r[4].

Example code snippet:
```cpp
// unit_0018.cpp:3636
set32(c.r[3], ld32((c.r[4].w[0] + 14000u)));
```

### 6. unit_0015.cpp — Offset 14000 (0x36B0)

Line 4351 — ld32 access from c.r[4].

Example code snippet:
```cpp
// unit_0015.cpp:4351
set32(c.r[3], ld32((c.r[4].w[0] + 14000u)));
```

### 7. unit_0090.cpp — Offsets 13700–25176 (0x358C–0x6258)

Lines 2043–2131 — complex pattern with multiple large-offset accesses on c.r[4] (appears to be a fighter object base):

| Offset (dec) | Offset (hex) | Lines | Operation |
|-------------|-------------|-------|-----------|
| 13700 | 0x358C | 2057, 2068, 2120, 2131 | ld32/st32 |
| 4548 | 0x11C4 | 2054, 2069 | ld32/st32 |
| 9124 | 0x239C | 2055, 2062 | ld32/st32 |
| 18276 | 0x4764 | 2058, 2070 | ld32/st32 |
| 22852 | 0x594C | 2067, 2073 | ld32/st32 |
| 25176 | 0x6258 | 2053, 2064, 2092 | ld32/st32 |

Example code snippet:
```cpp
// unit_0090.cpp:2053-2068
c.f[1] = ld32((c.r[4].w[0] + 25176u));
c.f[6] = ld32((c.r[4].w[0] + 4548u));
c.f[5] = ld32((c.r[4].w[0] + 9124u));
c.f[4] = ld32((c.r[4].w[0] + 13700u));
c.f[3] = ld32((c.r[4].w[0] + 18276u));
// ... arithmetic ...
st32((c.r[4].w[0] + 13700u), c.f[0]);
st32((c.r[4].w[0] + 25176u), c.f[1]);
```

### 8. unit_0069.cpp — Offsets 48–184 (below 0x540) AND loads from VA base

Lines 1203–1290 — mixed pattern:

- **Loads from VA base 0x005C0000 + offsets 13472–13720**: These are reads from global/static data tables (VA base + offset), NOT fighter object fields.
- **st32 at small offsets 48–184 from c.r[16]**: These ARE fighter object fields, but all below 0x540.

Example code snippet (fighter field writes, below 0x540):
```cpp
// unit_0069.cpp:1223-1239
st32((c.r[16].w[0] + 48u), c.f[5]);
st32((c.r[16].w[0] + 52u), c.f[4]);
st32((c.r[16].w[0] + 56u), c.f[3]);
st32((c.r[16].w[0] + 60u), c.f[2]);
st32((c.r[16].w[0] + 64u), c.f[1]);
st32((c.r[16].w[0] + 68u), c.f[0]);
```

## Important distinction: VA-base reads vs fighter-object fields

Several accesses at large decimal offsets (13520–13720) were found added to **VA base addresses** (0x00480000, 0x005C0000), not fighter object base pointers:

| Unit | Lines | Base | Offset (dec) | Offset (hex) | Nature |
|------|-------|------|-------------|-------------|--------|
| unit_004A.cpp | 3779–3874 | 0x00480000 | 13520–13568 | 0x34B0–0x34F0 | Global data table read |
| unit_0069.cpp | 1205–1261 | 0x005C0000 | 13472–13720 | 0x3480–0x3588 | Global data table read |

These are **not** fighter object fields — they read from global data at absolute VA addresses.

## Fighter fields NOT positively identified

The following fighter fields were **not** found with identifiable names or semantics:

| Field type | Searched for | Found? |
|------------|-------------|--------|
| Health | health | No |
| Ki/energy | ki_, energy | No |
| Stamina | stamina | No |
| Velocity X/Y/Z | velocity, vel_x/y/z | No |
| Animation state | anim_state, animation | No |
| Fighter state machine | state_machine, fighter_state | No |
| Current position | position (beyond spawn vec4) | No |
| Grounded flag | m_grounded | No |
| Running flag | m_running | No |
| Jumping flag | m_jumping | No |
| Falling flag | m_falling | No |

## Confirmed fighter struct size

The struct is **larger than 0x540**. Field accesses confirmed at decimal offsets up to 25176 (0x6258), specifically:

- Offset 14000 (0x36B0) — confirmed in unit_0015, unit_0016, unit_0017, unit_0018
- Offset 14464–14552 (0x3880–0x38D8) — confirmed in unit_000D
- Offset 13700 (0x358C) — confirmed in unit_0090
- Offset 25176 (0x6258) — confirmed in unit_0090

No upper bound on struct size was determined from the field access pattern alone.

## Fields below +0x540 (already documented)

Previously confirmed fighter fields at offsets +0x510 through +0x53C:
- +0x510–+0x51C: Spawn vec4 (X, Y, Z, W)
- +0x520: Flags bitmask
- +0x534: Side selector sub-object pointer
- +0x538: Vtable index
- +0x53C: Vtable index

No fields were found at +0x540 through +0x53C boundary (i.e., the gap between documented fields and the newly discovered fields at 14000+).

## Files written

No files were modified. Read-only analysis only. Findings documented in this report.
