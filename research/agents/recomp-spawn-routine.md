# Recompiled Spawn Routine Analysis — Super Dragon Ball Z

## 1. Location note (material correction)

The task cited `unit_004C.cpp` (VA `0x00230000`–`0x00233FFF`) as containing the SLES spawn routine at `0x23ACC8`. Verification shows this is **incorrect**:

- `0x23ACC8` − `0x00230000` = `0xACC8` = 44232, which is **outside** the unit range (max offset `0x3FFF` = 16383).
- `unit_004C.cpp` covers `[0x00230000, 0x00234000)` and is a state-machine unit (switch on a byte at object+0x30). It has **zero** spawn-signature accesses: no `+0x24`/field_24, no `0x534`/1332 side selector, no `stage_group_map`, no placement table.

The spawn routine is actually in **`unit_004E.cpp`**, which covers `[0x00238000, 0x0023C000)`. Entry label `L_0023AC90` (source line 3972) contains VA `0x23ACC8` (0x38 bytes in, immediately after the prologue, at the `field_08` load). All analysis below is read-only against `unit_004E.cpp`; no files in `ps2-web-recomp` were modified.

## 2. Spawn routine entry point

| Item | Recomp (unit_004E.cpp) | SLUS | SLES |
|---|---|---|---|
| Spawn routine entry | `L_0023AC90`, line 3972, VA `0x0023AC90` (prologue `sp-=368`, saves ra+s16..s30) | `0x0023AF30` | `0x0023ACC8` |
| Routine body end | line 4209 (epilogue `sp+=368; return`) | `0x0023B2B0` | `0x0023B220` |
| Spawn sub-routine entry (main, 14 callers) | `L_0023B140`, line 4286 | — | `0x0023B140` |
| Spawn alternate entry | `L_0023B020`, line 4210 | — | `0x0023B020` |
| Spawn alternate entry | `L_0023B0E0`, line 4258 | — | `0x0023B0E0` |
| Pre-spawn object resolver | `L_0023A430`, line 3367 | — | `0x0023A430` |

Argument convention (per research + decompiled code): `$a0` = stage object (`c.r[4]`), `$a1` = fighter/player object (`c.r[5]`). Called in pairs for two fighters from the same stage.

## 3. Section-by-section mapping (decompiled code ↔ documented SLUS/SLES addresses)

### 3.1 Stage key lookup: `field_08*2 + field_24` → `stage_group_map`

Source `unit_004E.cpp:3987-3993` (inside `L_0023AC90`):

```
c.r[5] = *(u32*)(stage + 8);      // stage->field_08  (stage ID 0-12)
c.r[2] = *(u32*)(stage + 36);     // stage->field_24  (variant bit 0/1)
c.r[3] = 0x480000 + 24304;        // = 0x485EF0  (SLES stage_group_map base; SLUS 0x00488400)
c.r[3] = 0x485EF0 + field_08*8;   // map base + field_08*8
c.r[2] = field_24*4;              // field_24*4
c.r[2] = 0x485EF0 + field_08*8 + field_24*4;  // &stage_group_map[field_08*2 + field_24]
```

- `stage+0x08` (field_08) and `stage+0x24` (field_24) match `s_game_stage +0x08/+0x24` ✓
- `map_index = field_08*2 + field_24` ✓
- SLES `stage_group_map` at `0x00485EF0` matches research exactly ✓ (SLUS `0x00488400`; uniform data shift `-0x2510`)

### 3.2 Slot selector: stage vtable `+0x28`

Source `unit_004E.cpp:3994`:
```
c.r[25] = *(u32*)(*(u32*)stage + 40);   // stage vtable +0x28
call c.r[25] with $a0 = stage;          // returns slot selector 0-4
```
- Vtable byte offset `+0x28` (= +40) = slot selector ✓ (SLUS/SLES `stage vtable+0x28`, returns slot 0–4)

### 3.3 Placement table access

Source `unit_004E.cpp:4000-4032` (inside `L_0023ACF0`, line 3997):

```
c.f[3] = *(0x480000 + 22644);   // = 0x485874  = 100.0f #1 (SLES; SLUS 0x00487D84)
c.f[0] = *(0x480000 + 22652);   // = 0x48587C  = 1.0f  #1 (SLES; SLUS 0x00487D8C)
group*5 << 4  = group*0x50      // group stride (line 4002)
slot << 4     = slot*0x10       // slot stride   (line 4003)
c.r[3] = 0x480000 + 22768;      // = 0x4858F0  = SLES placement table base (SLUS 0x00487E00)
record = 0x4858F0 + group*0x50 + slot*0x10   // 4 floats [x,y,z,w]
c.f[2] = *(0x480000 + 22660);   // = 0x485884  = 100.0f #2 (SLES; SLUS 0x00487D94)
c.f[0] = *(0x480000 + 22668);   // = 0x48588C  = 1.0f  #2 (SLES; SLUS 0x00487D9C)
```

- Placement table base `0x004858F0` (SLES) matches research ✓ (SLUS `0x00487E00`)
- Group stride `0x50` (16 groups), slot stride `0x10` (5 slots × 4 floats) ✓
- Four constants `100.0f/1.0f` pairs match SLUS/SLES exactly ✓

### 3.4 Candidate vector construction

Source `unit_004E.cpp:4033-4109`:
- Loads the selected slot's 4 floats; computes direction via `sub.s` (slot+2 − slot+0, slot+3 − slot+1); scales via `mul.s` by the direction factor; adds base via `add.s`.
- Builds two candidate position vectors (candidate A and candidate B), matching the research's `$sp+0x150` (A) / `$sp+0x160` (B) in SLUS terms.

### 3.5 Normalization helper call

Source `unit_004E.cpp:4114`:
```
unit_0001(c, 105u, ...);   // ra = L_0023AEC4 (line 4115)
else fallback pc = 0x00106830;   // SLES normalization helper
```
- Helper `unit_0001` entry 105 resolves to VA `0x00106830` = SLES vector normalization helper ✓ (SLUS `0x00106950`)
- Semantics `*a0 = *a0 / |*a0|` (uniform scale to unit length via `vmul.xyz → vaddy/vaddz → vrsqrt Q → vwaitq → vmulq.xyz`) ✓
- Normalized direction is then scaled/positioned into the two candidate vectors.

### 3.6 Side selector: `player->0x534->0x08`

Source `unit_004E.cpp:4169`:
```
unit_0032(c, 415u, ...);   // ra = L_0023AF9C (line 4170)
else fallback pc = 0x001CB9D0;   // SLES side selector helper
```
- Helper `unit_0032` entry 415 resolves to VA `0x001CB9D0` = SLES side selector ✓ (SLUS `0x001CBB40`)
- Implements `player->0x534->0x08` ($a0 = player object `c.r[21]`) ✓
- Selection: return `1` → candidate B (`L_0023AFD4`, line 4188); return `0` → candidate A (`L_0023AFB8`, line 4178); other nonzero → candidate B (line 4177) ✓

### 3.7 Player position setter: player vtable `+0x40`

Source `unit_004E.cpp:4178-4194`:
```
c.r[25] = *(u32*)player;                      // player vtable ptr
c.r[25] = *(u32*)(vtable + 64);               // player vtable +0x40 = position setter
c.r[4] = player;                              // $a0 = player
c.r[5] = sp + offset;                         // $a1 = pointer to selected candidate vector
call c.r[25];                                  // player_vmethod(+0x40)(player, vec3*)
```
- Vtable byte offset `+0x40` (= +64) = world-space position setter ✓ (SLUS/SLES `fighter vtable+0x40`)
- Selected candidate vector passed as the position argument ✓

## 4. Reconstructed class-layout verification

| Layout field | Documented semantics | Verified in decompiled spawn routine | Source line |
|---|---|---|---|
| `s_game_stage +0x08` | stage ID (0–12) | loaded `stage+8` for map index | 3987 |
| `s_game_stage +0x24` | stage variant/sub-stage bit (0/1) | loaded `stage+36` for map index | 3988 |
| `s_game_stage vtable +0x28` | slot selector (0–4) | loaded `vtable+40`, called | 3994 |
| `s_fighter vtable +0x28` | slot selector (stage's vmethod) | same as above | 3994 |
| `s_fighter vtable +0x40` | position setter `set_pos(player*, vec3*)` | loaded `vtable+64`, called with player + candidate vec | 4181/4192 |
| `s_fighter +0x534 → +0x08` | side selector sub-object → P1/P2 pick | via `unit_0032` helper (player->0x534->0x08) | 4169 |
| `s_fighter +0x520` | flags bitmask | confirmed adjacent routine `fighter+1312 (=+0x520)` checked `& 256` | 5041 |
| `s_fighter +0x510` | spawn/home vec4 | written by post-spawn init (`0x1CD9D0`), not the placement routine itself | research |
| `s_fighter +0x538/+0x53c` | vtable index / mirror | fighter lifecycle init, not the placement routine itself | research |
| `s_game_session +0x58` | stage pointer | caller-side: stage resolved from session+0x58, passed in $a0 | research |
| `s_game_session +0x60/+0x64` | fighter #1 / #2 pointers | caller-side: fighters resolved from session+0x60/+0x64, passed in $a1 | research |
| `s_fighter vtable +0x4c/+0x78/+0xac` | orientation / state config / post-placement | post-spawn init (`0x30E300`) runs after the placement routine | research |

All layout fields that the placement routine directly accesses match the reconstruction. The session offsets (`+0x58/+0x60/+0x64`) and fighter `+0x510/+0x538/+0x53c` are touched by the callers/adjacent lifecycle routines (pre-spawn resolver `L_0023A430`, post-spawn init), which pass the already-resolved stage (`$a0`) and fighter (`$a1`) into the placement routine — consistent with the research's documented calling convention.

## 5. Data-address mapping summary (SLES ↔ SLUS)

| Data | SLES (this unit) | SLUS | Delta | Verified in code |
|---|---|---|---|---|
| `stage_group_map` | `0x00485EF0` | `0x00488400` | `-0x2510` | line 3991 (`0x480000+24304`) |
| Placement table | `0x004858F0` | `0x00487E00` | `-0x2510` | line 4026 (`0x480000+22768`) |
| Constant 100.0f #1 | `0x00485874` | `0x00487D84` | `-0x2510` | line 4000 |
| Constant 1.0f #1 | `0x0048587C` | `0x00487D8C` | `-0x2510` | line 4014 |
| Constant 100.0f #2 | `0x00485884` | `0x00487D94` | `-0x2510` | line 4039 |
| Constant 1.0f #2 | `0x0048588C` | `0x00487D9C` | `-0x2510` | line 4044 |
| Normalization helper | `0x00106830` | `0x00106950` | `-0x0120` | line 4114 fallback pc |
| Side selector helper | `0x001CB9D0` | `0x001CBB40` | `-0x0170` | line 4169 fallback pc |
| Spawn routine | `0x0023ACC8` (body `L_0023AC90`) | `0x0023AF30` | `-0x0268` | unit_004E.cpp line 3972 |

## 6. Conclusion

The decompiled spawn routine in `unit_004E.cpp` (entry `L_0023AC90`, containing SLES `0x23ACC8`) implements the full stage→fighter placement pipeline documented in the ELF research: stage key lookup (`field_08*2 + field_24` into `stage_group_map` at SLES `0x00485EF0`), stage vtable `+0x28` slot selector, placement table at SLES `0x004858F0` (group stride `0x50`, slot stride `0x10`), two candidate vectors, COP2 normalization helper (SLES `0x00106830`), side selector `player->0x534->0x08` (SLES `0x001CB9D0`) choosing candidate A/B, and player vtable `+0x40` position setter. Every field offset, table address, constant, and helper address matches the SLUS/SLES research exactly (uniform data shift `-0x2510`; non-uniform code shifts `-0x0268`/`-0x0120`/`-0x0170`). Reconstructed class layouts (`s_game_stage +0x08/+0x24`, `s_fighter_vtable +0x28/+0x40`, `s_fighter +0x534→0x08`, `s_fighter +0x520`) are confirmed by the decompiled accesses.
