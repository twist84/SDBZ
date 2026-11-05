# Super Dragon Ball Z — ELF Spawn Research Log

## Scope

Investigate original player placement in the PlayStation 2 executable and reproduce the original stage-to-player placement logic in the native rebuild. This log records observable engineering findings, evidence, hypotheses, and unresolved work. It is not a private chain-of-thought transcript.

## Input executable

- File: `SLUS_214(2).42` (also at `assets/original/SLUS_214.42`)
- Format: ELF32, little-endian
- Machine: MIPS R3000 / MIPS III flags
- Linked: statically linked, not stripped
- File size: 4,196,528 bytes
- ELF entry point: `0x00100008`
- Load segment: virtual address `0x00100000`, file offset `0x80`, file size `0x400680`

For addresses in the load segment:

`file_offset = virtual_address - 0x00100000 + 0x80`

## Confirmed target routine: `0x0023AF30`

The routine begins at `0x0023AF30` and returns at `0x0023B2B0`.

### Calling convention evidence

- `$a0` is used as the stage-like object (`lw` fields at `+0x08`, `+0x24`, and vtable pointer at `+0x00`).
- `$a1` is copied to `$s5` and later used as the player-like object.
- The routine uses a large stack frame (`0x170` bytes).

### Object fields and virtual calls

| Evidence | Interpretation |
|---|---|
| `lw $5, 0x08($a0)` at `0x23AF68` | Stage object field at `+0x08` = stage ID (0–12 for 13 stages); feeds stage-group map index |
| `lw $2, 0x24($a0)` at `0x23AF6C` | Stage object field at `+0x24` = variant/sub-stage bit (0/1); feeds stage-group map index |
| `lw $t9, 0x28($t9)` at `0x23AF84` | Calls stage object's virtual method at vtable byte offset `+0x28` — spawn-slot selector returning 0–4 |
| `move $a0, $s5` at `0x23B1DC` | Player object becomes first argument |
| `lw $t9, 0x40($t9)` at `0x23B260` / `0x23B27C` | Calls player object's virtual method at vtable byte offset `+0x40` — world-space 3-float position setter |
| `addiu $a1, $sp, 0x160` at `0x23B268` | One candidate 3-float position vector passed to player method |
| `addiu $a1, $sp, 0x150` at `0x23B284` | Alternate candidate 3-float position vector passed to player method |

The player position method is selected by a result from helper `0x001CBB40`:
- Return value `1`: use vector at stack `+0x150`.
- Return value `0`: use vector at stack `+0x160`.
- Other values: branch to the `+0x160` path.

This return-value interpretation follows the branch sequence at `0x23B23C`–`0x23B288`.

Helper `0x001CBB40` is a two-instruction nested field accessor (not a collision query):
```asm
001cbb40: lw   $v0, 0x534($a0)
001cbb44: jr   $ra
001cbb48: lw   $v0, 0x08($v0)
```
It returns `player->0x534->0x08`, a binary side/player-index selector (P1 vs P2) choosing candidate vector A or B.

### Position construction

The routine reads several global float values and indexed table entries, then builds candidate vectors in stack locals. It also calls helper `0x00106950` (vector normalization — see below) and `0x001CBB40`.

**Helper `0x00106950` — Vector Normalization (RESOLVED):**

This helper was previously undecoded due to COP2/MIPS-3D instructions. It is now fully decoded:

```asm
0x00106950: addiu $sp, $sp, -0x10
0x00106954: lwc1  $f3, ($a0)           ; load float[0]
0x00106958: addiu $v1, $sp, 0
0x0010695C: lwc1  $f2, 4($a0)          ; load float[1]
0x00106960: lwc1  $f1, 8($a0)          ; load float[2]
0x00106964: lwc1  $f0, 0xC($a0)        ; load float[3]
0x00106968: swc1  $f3, ($sp)           ; store to stack
0x0010696C: swc1  $f2, 4($sp)
0x00106970: swc1  $f1, 8($sp)
0x00106974: swc1  $f0, 0xC($sp)
0x00106978: ldc2  $4, ($v1)            ; load into COP2 vf04
0x0010697C: vmul.xyz vf05,vf04,vf04    ; 0x4BC4216A — square (component-wise)
0x00106980: vaddy vf05,vf05,vf05      ; 0x4B052941 — sum of squares (y)
0x00106984: vaddz vf05,vf05,vf05      ; 0x4B052942 — sum of squares (z)
0x00106988: vrsqrt Q,vf00w,vf05x      ; 0x4A6503BE — Q = 1/|v|
0x0010698C: vwaitq                     ; 0x4A0003BF — wait for Q
0x00106990: vmulq.xyz vf04,vf04,Q      ; 0x4BC0211C — scale to unit length
0x00106994: sdc2  $4, ($v1)           ; store back
0x00106998: lwc1  $f3, ($sp)           ; reload
0x0010699C: lwc1  $f2, 4($sp)
0x001069A0: lwc1  $f1, 8($sp)
0x001069A4: lwc1  $f0, 0xC($sp)
0x001069A8: swc1  $f3, ($a0)           ; write back
0x001069AC: swc1  $f2, 4($a0)
0x001069B0: swc1  $f1, 8($a0)
0x001069B4: swc1  $f0, 0xC($a0)
0x001069B8: jr    $ra
0x001069BC: addiu $sp, $sp, 0x10
```

Net: `*a0 = *a0 / |*a0|` — **uniform unit-length normalization**. This is NOT a matrix/rotation transform. The spawn routine builds a difference/direction vector from table floats, normalizes it, then constructs two candidate position vectors using the global constants.

### Stage-group map and placement table

The table lookup at the start of `0x0023AF30` is now structurally decoded:

```c
u32 *stage_group_map = (u32 *)0x00488400;
u32 map_index = stage_object->field_08 * 2 + stage_object->field_24;
u32 group_index = stage_group_map[map_index];
```

The first 28 map entries (from ELF `0x00488400`, verified):
```
index:  00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
value:   0  0  1  1  2  2  3  4  5  6  7  8  9  9 10 10 11 11 12 12 13 13 14 14 15 15  0  0
```

Entries 0–23 are confirmed meaningful (selecting one of 16 placement groups 0–15). Entries 24–27 (15, 15, 0, 0) may be adjacent unrelated data.

The coordinate-like data starts at `0x00487E00`. The routine computes:
```c
u32 group_byte_offset = group_index * 0x50; // 80 bytes
u32 slot_byte_offset = stage_virtual_method_result * 0x10; // 16 bytes
u32 record_offset = group_byte_offset + slot_byte_offset;
```

Four float arrays are addressed at:
- `0x00487E00 + record_offset`
- `0x00487E04 + record_offset`
- `0x00487E08 + record_offset`
- `0x00487E0C + record_offset`

Equivalently, the data is laid out as 16 groups × 5 slots × 4 floats = 320 floats (0x500 bytes), ending at `0x004883FF`. The `0x00488400` map immediately follows it.

### Sample raw slot values

The 20 floats (five slots × four floats) for each group are available in the binary. A few examples:

| Group | Slot 0 | Slot 1 | Slot 2 |
|---:|---|---|---|
| 1 | `20.3, 66.0, 14.5, 34.4` | `-175.5, 204.7, -201.1, 208.0` | `-182.6, -80.4, -141.7, -91.9` |
| 2 | `-85.0, 70.0, -85.0, 40.0` | `-335.9, -194.0, -307.3, -182.3` | `-67.3, -183.7, -37.1, -185.7` |
| 5 | `-15.0, 0.0, 15.0, 0.0` | `104.7, 234.4, 82.7, 260.6` | `174.5, -41.8, 204.4, -36.8` |
| 7 | `-3.2, 6.9, 22.4, -3.4` | `-113.7, 242.8, -96.9, 222.7` | `279.9, -50.8, 272.9, -80.5` |

Some groups/slots contain repeated sentinel-looking values such as `-15, 0, 15, 0`; do not treat those as valid world positions without understanding the caller's slot selection and the later calculation.

### Global constants used by the calculation

The constants at `0x00487D84`, `0x00487D8C`, `0x00487D94`, and `0x00487D9C` decode as:
- `0x00487D84`: `100.0f`
- `0x00487D8C`: `1.0f`
- `0x00487D94`: `100.0f`
- `0x00487D9C`: `1.0f`

These are used to initialize stack-local values in the vector construction. Their semantic role (scale, coordinate baseline, or homogeneous component) is not yet confirmed.

### Coordinate convention (best interpretation)

- Four-float slot = `[x, y, z, w]`; float[1] = vertical Y.
- Y-up, right-handed coordinate system; ground at y=0; w ≈ 0 (padding/homogeneous component).
- Evidence: group 13 slot 1/2 fighters at y=0 exactly, w=0, on opposite corners `[-165, 0, -135]` / `[135, 0, 165]`.
- Possible /100 scale hinted by global 100.0f constant, but transform not confirmed.

### Sentinel vs real groups

Sentinel (all 5 slots identical): groups 0, 3, 4, 9, 10, 15 (group 15 uses `{-15, 0.1, 15, 0.1}`).
Real data: groups 1, 2, 5, 6, 7, 8, 11, 12, 13, 14.

### Stage-to-placement-group mapping

Stage field +0x08 = stage ID (0–12 for 13 stages); field +0x24 = variant (0/1). Map index = `stage_id * 2 + variant`.

| Stage | field_08 | Variant 0 → Group | Variant 1 → Group | Data |
|---|---|---|---|---|
| s00 | 0 | 0 (sentinel) | 0 (sentinel) | none |
| s01 | 1 | 1 (real) | 1 (real) | real |
| s02 | 2 | 2 (real) | 2 (real) | real |
| s03 | 3 | 3 (sentinel) | 4 (sentinel) | none |
| s04 | 4 | 5 (real) | 6 (real) | real |
| s05 | 5 | 7 (real) | 8 (real) | real |
| s06 | 6 | 9 (sentinel) | 9 (sentinel) | none |
| s07 | 7 | 10 (sentinel) | 10 (sentinel) | none |
| s08 | 8 | 11 (real) | 11 (real) | real (=g1 data, no .stg) |
| s09 | 9 | 12 (real) | 12 (real) | real (=g2 data, no .stg) |
| s10 | 10 | 13 (real) | 13 (real) | real (slots 1,2 only) |
| s11 | 11 | 14 (real) | 14 (real) | real (=g1 data) |
| s12 | 12 | 15 (sentinel) | 15 (sentinel) | none |

Authoritative stage list from decoded INFO.DAT: 13 STG records (s00, s01, s02, s03a, s03b, s04a, s04b, s05, s06, s07, s10, s11, s12). s08/s09 have models/textures but no .stg/.hit files.

Stages with real placement: s01, s02, s04, s05, s08, s09, s10, s11 (8 stages).
Sentinel/none: s00, s03, s06, s07, s12 (5 stages).

### Callers of `0x0023AF30`

All callers follow the same pattern: stage=session object at `owner+0x58`, first fighter at `owner+0x60`, second fighter at `owner+0x64`. Two consecutive `0x23AF30` calls (one per fighter) followed by `0x30E300` per fighter.

| Caller | Calls | Config calls | Lifecycle |
|---|---|---|---|
| `0x00298180` | `0x29820C`, `0x298218` | loop `$s0` 0–45 gated on `fighter+0x528==29` | fight init |
| `0x00392D80` | `0x392DF4`, `0x392E00` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x33,0)` | fight init |
| `0x003956A0` | `0x395728`, `0x395734` | `0x1cbe80(f1,0x20,0)`, `0x1cbe80(f2,0x24,0)`, `0x1cd9d0(f2,0x1ca,0)` | fight init |
| `0x00398910` | `0x398978`, `0x398984` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x25,0)`, `0x1cd9d0(f2,0xb4,0)` | fight init / rematch |
| `0x00399640` | `0x3996F0`, `0x3996FC` | same as `0x3956A0` pattern | fight init |
| `0x0039B550` | `0x39B59C`, `0x39B5A8` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x33,0)` | fight init |
| `0x0039DD40` | `0x39DDB4`, `0x39DDC0` | `0x1cbe80(f1,0x25,0)`, `0x1cbe80(f2,0x25,0)`, `0x1cd9d0(f1,0xb4)`, `0x1cd9d0(f2,0xb5)` | rematch / round-reset |
| `0x0039FE00` | `0x39FE7C`, `0x39FE8C` | `0x1cbe80(f1,0,0)`, `0x1cbe80(f2,0x25,0)`, `0x1cd9d0(f2,0xb4,0)` | fight init / rematch |

**Lifecycle:** Two-fighter placement at fight initialization (stage load / round setup), not mid-round respawn. Callers with `0x1cd9d0(0xb4/0xb5)` (`0x39DD40`, `0x398910`, `0x39FE00`) indicate rematch / round-reset re-placement path.

### Important unresolved interpretation

The stage virtual method result is multiplied by 16 and therefore selects one of five 16-byte records. It is not yet proven whether the four floats are two fighter positions, rectangle bounds, endpoints, or another placement descriptor. The later arithmetic derives differences between selected components and calls `0x00106950` (normalization), whose output is then used to build two candidate vectors via scalar `mul.s`/`add.s` with globals 100.0f/1.0f.

The exact scalar arithmetic between table reads and normalization, and between normalization and candidate vector construction, is partially decoded from the capstone disassembly but not fully verified. No world-coordinate mapping should be implemented until the four-float record and transform are fully understood.

### Updated confidence

- High: stage fields `+0x08` (stage ID) and `+0x24` (variant) form `field_08 * 2 + field_24` into a 32-bit map at `0x00488400`.
- High: map values select one of 16 groups in a 16 × 5 × 4-float table at `0x00487E00`.
- High: stage vtable method `+0x28` returns a value used as a five-slot selector (stride 16 bytes).
- High: helper `0x00106950` normalizes a 4-float vector to unit length (COP2/MIPS-3D).
- High: `0x001CBB40` returns `player->0x534->0x08` (nested field accessor, P1 vs P2 side selector).
- High: player vtable method `+0x40` is a world-space 3-float position setter.
- Medium: each 16-byte slot is a four-float placement descriptor in `[x, y, z, w]` format, Y-up.
- Medium: `field_24` is a variant/sub-stage bit; exact semantic meaning not confirmed.
- Unknown: exact scalar math between table reads and normalization, and between normalization and candidate vector construction.
- Unknown: exact slot selection per stage (stage vtable `+0x28` return domain).

### Next analysis targets (updated)

1. Reconstruct the stack-vector math from `0x23AF90` through `0x23B234` symbolically, preserving each input float's table offset, now that normalization is confirmed.
2. Correlate the 16 placement groups with stage IDs/names and validate selected slots against known stage geometry or emulator observations.
3. Identify the player vtable method at `+0x40` and confirm the setter's coordinate convention.
4. Compare all callers to identify which are round start, stage transition, rematch, or respawn paths.
5. Validate the reconstructed positions against known stage/player placements before changing native spawn selection.

### Implementation consequence

Do not implement two hard-coded side coordinates per stage. The correct native port should eventually reproduce this sequence:
1. Derive the stage placement group from the two stage fields using the 24-entry map (field_08 = stage ID, field_24 = variant).
2. Ask the stage object equivalent for the slot selector (0–4) via vtable +0x28.
3. Read the selected four-float descriptor from the table at `0x00487E00`.
4. Reproduce the original descriptor-to-two-vectors calculation (build direction vector, normalize via unit-length normalization, construct candidate positions using constants 100.0f/1.0f).
5. Select the vector using the fighter-side field equivalent (player->0x534->0x08: 1 → candidate B, 0 → candidate A).
6. Pass the selected position to the fighter position setter (vtable +0x40).

The descriptor transform is now understood at a higher level (normalization), but the exact scalar arithmetic still needs symbolic reconstruction. The fighter-side field and stage vtable +0x28 return domain also remain to be confirmed against native stage data.

## Evidence artifacts

- Disassembly generated with custom Python MIPS+COP2/MIPS-3D disassembler (replicating PCSX2's DisR5900asm.cpp tables + binutils-gdb R5900 VU0 encodings).
- Function range inspected: `0x0023AF30` through `0x0023B2B4`.
- Helper `0x00106950` fully decoded: 6 COP2 instructions → vector normalization.
- Caller analysis: 8 callers identified with field semantics.
- Stage correlation: 13 stages mapped to 16 placement groups.
- Full decoded disassembly and table dumps in worktree artifacts.
- ELF: `assets/original/SLUS_214.42` (4,196,528 bytes).

## Follow-up: helper correction and call sites

### Corrected interpretation of `0x001CBB40`

The helper at `0x001CBB40` is only two instructions plus a delay-slot load:

```asm
001cbb40: lw   $v0, 0x534($a0)
001cbb44: jr   $ra
001cbb48: lw   $v0, 0x08($v0)
```

Therefore it is **not** a collision query. In `0x0023AF30`, `$a0` is the player object at the time of the call. The helper returns the 32-bit value at `player->0x534->0x08`; `0x23AF30` uses that value to select one of the two candidate vectors. The value is a binary side/player-index selector (P1 vs P2).

### Confirmed call sites

A full executable disassembly found multiple direct calls to `0x0023AF30`:
- `0x0029820C`, `0x00298218`
- `0x00392DF4`, `0x00392E00`
- `0x00395728`, `0x00395734`
- `0x00398978`, `0x00398984`
- `0x003996F0`, `0x003996FC`
- `0x0039B59C`, `0x0039B5A8`
- `0x0039DDB4`, `0x0039DDC0`
- `0x0039FE7C`, with further call pairs in the same region

The repeated paired calls strongly indicate the routine is used to place two fighter/player objects for a stage/session setup, rather than being a one-off stage-load-only helper.

### Caller evidence: `0x00298180`

At `0x00298180` the caller obtains two objects using a helper with selector values `0` and `1`, stores them in `$s2` and `$s1`, then invokes:
```asm
0x00298208: move $a0, $s3
0x0029820C: jal  0x0023AF30
0x00298210: move $a1, $s2
0x00298214: move $a0, $s3
0x00298218: jal  0x0023AF30
0x0029821C: move $a1, $s1
```
This confirms the same stage/session object is passed with two distinct fighter objects.

### Caller evidence: `0x00392D80`

The caller loads an owning object from `[$a0 + 0x04]`, accesses stage/session at `+0x58`, and fighter objects at `+0x60` and `+0x64`. It calls `0x23AF30` once for each fighter. This is further evidence that `0x23AF30` is a reusable two-fighter placement routine called during higher-level setup/transition flows.

### Current confidence

- High: `0x23AF30` receives a stage/session object and a fighter object.
- High: it builds two candidate 3-float vectors and calls the fighter's virtual method at vtable `+0x40` with the selected vector.
- High: it is called in pairs for the two fighters.
- High: helper `0x00106950` normalizes a vector to unit length (COP2/MIPS-3D).
- High: `0x001CBB40` returns `player->0x534->0x08` (side selector).
- High: stage field `+0x08` = stage ID (0–12), `+0x24` = variant bit.
- High: coordinate convention is `[x, y, z, w]`, Y-up, ground at y=0.
- Medium: stage fields select a placement group through the 24-entry map.
- Medium: map values select one of 16 groups in a 16 × 5 × 4-float table at `0x00487E00`.
- Medium: stage vtable method `+0x28` returns a value used as a five-slot selector (stride 16 bytes).
- Medium: each 16-byte slot is a four-float placement descriptor.
- Unknown: exact scalar math between table reads and normalization, and between normalization and candidate vector construction.
- Unknown: exact slot selection per stage (stage vtable `+0x28` return domain).
- Unknown: semantic meaning of the nested player field `+0x534 -> +0x08` beyond P1/P2 side selection.

### Next analysis targets (updated)

1. Reconstruct the stack-vector math from `0x23AF90` through `0x23B234` symbolically, preserving each input float's table offset, now that normalization is confirmed.
2. Identify the player vtable method at `+0x40` and confirm the setter's coordinate convention.
3. Compare all callers to identify which are round start, stage transition, rematch, or respawn paths.
4. Validate the reconstructed positions against known stage/player placements before changing native spawn selection.

### Implementation consequence

Do not implement two hard-coded side coordinates per stage. The correct native port should eventually reproduce this sequence:

1. Derive the stage placement group from the two stage fields using the 24-entry map (field_08 = stage ID, field_24 = variant).
2. Ask the stage object equivalent for the slot selector (0–4) via vtable +0x28.
3. Read the selected four-float descriptor from the table at `0x00487E00`.
4. Reproduce the original descriptor-to-two-vectors calculation (build direction vector, normalize via unit-length normalization, construct candidate positions using constants 100.0f/1.0f).
5. Select the vector using the fighter-side field equivalent (player->0x534->0x08: 1 → candidate B, 0 → candidate A).
6. Pass the selected position to the fighter position setter (vtable +0x40).

The descriptor transform is now understood at a higher level (normalization), but the exact scalar arithmetic still needs symbolic reconstruction. The fighter-side field and stage vtable +0x28 return domain also remain to be confirmed against native stage data.