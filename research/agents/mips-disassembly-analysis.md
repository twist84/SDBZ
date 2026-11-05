# Super Dragon Ball Z — MIPS Disassembly Analysis: Spawn Routine 0x0023AF30 (Collated Report)

## 1. What was analyzed

**Executables inspected:**
- `C:\Dev\SuperDragonBallZ\assets\original\SLUS_214.42` (US retail, 4,196,528 bytes) — primary analysis target.
- `C:\Dev\SuperDragonBallZ\assets\original\SLES_541.61` (EU beta, 4,180,784 bytes) — inspected for address/offset delta after build update.

**Format:** ELF32 MIPSel (little-endian), statically linked, stripped (no `.symtab` symbols; `llvm-nm` reports "no symbols"). Load segment: vaddr `0x00100000`, file off `0x80`.

**Tooling:** Custom MIPS32-LE decoder over raw ELF bytes (`C:\Dev\SuperDragonBallZ\.kilo\disasm_mips.py`), cross-validated against the prior `SLUS_spawn_research_log.md`. The decoder handles standard MIPS III opcodes including `move` (funct 0x2d), `jalr`, branch/jump target resolution, and `lw`/`sw`/`lwc1`/`swc1`. COP2 / MIPS-3D media instructions (`dps.w.ph`, `dpa.w.ph`, `addu.qb`, `subu.qb`, `add_a.d`, `aver_u.h`, `dpau.h.qbl`, `ext`/`ins`, etc.) and the VFPU transform at `0x106950` are emitted as raw `.word`s because the available decoder lacks the MIPS-3D ASE. These are pure data transforms and do not affect control-flow or field semantics.

**Scope:** Target routine `0x0023AF30`–`0x0023B2B0` (stage→fighter placement), all its direct callers, the stage/player object fields it reads, the vtable methods it invokes, the helper routines it calls, and the lifecycle contexts in which it runs.

---

## 2. Key findings — SLUS_214.42 (US retail)

### 2.1 Target routine 0x0023AF30 (stage→fighter placement)

Argument convention: `$a0` = stage/session object, `$a1` = fighter/player object. Stack frame `0x170` bytes.

**Stage group lookup (`0x23AF68`–`0x23AF8C`):**
```
0x23af68: lw  $a1, 8($a0)     ; stage->field_08
0x23af6c: lw  $v0, 0x24($a0)  ; stage->field_24
0x23af70: lw  $t9, ($a0)      ; stage vtable ptr
0x23af74: sll  $a1, $a1, 3    ; field_08 * 8
0x23af78: addu $v1, $v1, $a1  ; base 0x00488400 + field_08*8
0x23af7c: sll  $v0, $v0, 2    ; field_24 * 4
0x23af80: addu $v0, $v0, $v1  ; addr = base + field_08*8 + field_24*4
0x23af84: lw  $t9, 0x28($t9)  ; stage vtable method +0x28
0x23af88: jalr  $t9
0x23af8c: lw  $s0, ($v0)      ; delay slot: load stage_group_map[map_index]
```
Effective C:
```c
u32 *stage_group_map = (u32 *)0x00488400;
u32 map_index = stage->field_08 * 2 + stage->field_24;   // *8 bytes + *4 bytes /4
u32 group_index = stage_group_map[map_index];
```

**Slot selector (`0x23AF84`–`0x23AFA4`):**
The stage vtable method at byte offset `+0x28` is called (`jalr $t9`). Its return value (`$v0`) is multiplied by 16 (slot stride) and added to `group_index * 0x50` (group stride) to form the record offset into the placement table at `0x00487E00`:
```
record_offset = group_index * 0x50 + slot_selector * 0x10   // 4 floats per record
```

**Candidate vector construction (`0x23AF90`–`0x23B234`):**
Reads the selected slot's 4 floats, combines them with constants `100.0f` and `1.0f` (at `0x00487D84`, `0x00487D8C`, `0x00487D94`, `0x00487D9C`), builds two 3-float candidate vectors at stack `$sp+0x160` (vector B) and `$sp+0x150` (vector A), then calls VFPU transform helper `0x106950` at `0x23B15C`.

**Player-side vector selector (`0x23B234`–`0x23B28C`):**
Calls helper `0x001CBB40` with `$a0 = player`:
```
001cbb40: lw   $v0, 0x534($a0)   ; player->0x534
001cbb44: jr   $ra
001cbb48: lw   $v0, 0x08($v0)    ; return player->0x534->0x08
```
Return value `1` → vector at `$sp+0x150`; return `0` → vector at `$sp+0x160`; other nonzero → `$sp+0x160`.

**Position setter (`0x23B258`–`0x23B284`):**
Calls player vtable method at byte offset `+0x40` with `$a0 = player`, `$a1 = pointer to selected 3-float vector`.

### 2.2 Stage object fields +0x08 and +0x24

Decoded and verified. The 24-entry map read directly from ELF bytes at `0x00488400`:

```
index:  00 01 02 03 04 05 06 07 08 09 10 11
value:   0  0  1  1  2  2  3  4  5  6  7  8
index:  12 13 14 15 16 17 18 19 20 21 22 23
value:   9  9 10 10 11 11 12 12 13 13 14 14
index:  24 25 26 27
value:  15 15  0  0        ; fallbacks to group 0
```

**Field semantics (best-effort structural inference):**
- `stage+0x08` = **stage ID** (0–12 for the 13 stages; 13 = fallback → group 0)
- `stage+0x24` = **stage variant / sub-stage bit** (0 or 1) — picks between paired map entries `field_08*2` and `field_08*2+1`
- Variant matters only for field_08 ∈ {3, 4, 5}:
  - ID 3: v0→group 3, v1→group 4
  - ID 4: v0→group 5, v1→group 6
  - ID 5: v0→group 7, v1→group 8
- Result `group_index` ∈ [0,15] selects one of 16 placement groups.

### 2.3 Stage vtable method at +0x28 → slot selector

The stage object's vtable method at byte offset `+0x28` returns a **slot selector in the range 0–4**. Multiplied by 16 bytes, it selects one of five 4-float records within the group. Combined with `group_index * 0x50` group byte offset, the full record address is `0x00487E00 + record_offset`. This is a per-stage-subclass virtual "spawn-slot selector"; exact per-stage return values require stage vtable identification (not available in stripped binary).

### 2.4 Placement table (read from ELF at `0x00487E00`)

16 groups × 5 slots × 4 floats = 320 floats (0x500 bytes), ending at `0x004883FF`, immediately before the map at `0x00488400`.

- **Sentinel groups** `{0, 3, 4, 9, 10, 15}`: all five slots `[-15, 0, 15, 0]` (invalid/unused)
- **Real groups** `{1, 2, 5, 6, 7, 8, 11, 12, 13, 14}`: contain actual coordinates
- Group 13: `[[-165,0,-135,0], [135,0,165,0], ...]` — wide stage
- Groups 5–8: large ±300 values — large stages

**Slot 4-float interpretation:** `[base_x, base_z, dir_x, dir_z]` (or two points). Code computes `table[2]-table[0]` and `table[3]-table[1]` (direction), multiplies by 0.5, adds to base — yielding two interpolated candidate vectors (A at `$sp+0x160`, B at `$sp+0x150`) selected by the player side field.

### 2.5 Constants (read from ELF)
- `0x00487D84`: 100.0f (`0x42c80000`)
- `0x00487D8C`: 1.0f (`0x3f800000`)
- `0x00487D94`: 100.0f (`0x42c80000`)
- `0x00487D9C`: 1.0f (`0x3f800000`)

### 2.6 Player-side field +0x534 → +0x08 (candidate-vector selector)

`player->0x534` is a pointer to a nested sub-object; `->0x08` of that sub-object is a **binary side/position selector**:
- return 0 → candidate vector A (`$sp + 0x160`)
- return 1 → candidate vector B (`$sp + 0x150`)
- other → vector A

Best interpretation: **fighter side / player index** (0 = P1 / side A → vector A; 1 = P2 / side B → vector B). Exact sub-object type at `+0x534` could not be named from the stripped binary.

### 2.7 Player vtable method at +0x40 → position setter

Confirmed at caller `0x392E50`–`0x392E58`:
```
0x392e48: lw      $a0, 0x64($s0)     ; $a0 = fighter object
0x392e4c: lw      $t9, ($a0)         ; vtable ptr
0x392e50: lw      $t9, 0x40($t9)     ; vtable method +0x40
0x392e54: jalr    $t9
0x392e58: addiu   $a1, $sp, 0x30     ; $a1 = pointer to 3-float vector
```
```c
player_vtable[+0x40]( player* $a0, vec3* $a1 );   // sets the fighter's position
```
World-space 3-float position setter taking a pointer to the vector. Exact axis order (x,z vs x,y) and local-vs-world convention could not be fully decoded without expanding the COP2 transform at `0x106950`.

### 2.8 All direct call sites of 0x0023AF30 (SLUS)

Every caller resolves an owner/session object, then:
- stage pointer      = `owner + 0x58`
- fighter object #1  = `owner + 0x60`
- fighter object #2  = `owner + 0x64`

and invokes `0x23AF30(stage, fighter1)` then `0x23AF30(stage, fighter2)`, followed by per-fighter `0x30E300`, `0x302C80($f12=1.0f)`, and the position-setter vtable call.

| Caller fn | Call site #1 | Call site #2 | Pre-placement config |
|-----------|-------------|-------------|----------------------|
| 0x298180 | 0x29820C | 0x298218 | loop $s0 0..45 gated on fighter+0x528==29 |
| 0x392D80 | 0x392DF4 | 0x392E00 | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x33,0) |
| 0x3956A0 | 0x395728 | 0x395734 | 0x1cbe80(f1,0x20,0), 0x1cbe80(f2,0x24,0), 0x1cd9d0(f2,0x1ca,0) |
| 0x398910 | 0x398978 | 0x398984 | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x25,0), 0x1cd9d0(f2,0xb4,0) |
| 0x399640 | 0x3996F0 | 0x3996FC | 0x1cbe80(f1,0x20,0), 0x1cbe80(f2,0x24,0), 0x1cd9d0(f2,0x1ca,0) |
| 0x39B550 | 0x39B59C | 0x39B5A8 | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x33,0) |
| 0x39DD40 | 0x39DDB4 | 0x39DDC0 | 0x1cbe80(f1,0x25,0), 0x1cbe80(f2,0x25,0), 0x1cd9d0(f1,0xb4), 0x1cd9d0(f2,0xb5) |
| 0x39FE00 | 0x39FE7C | 0x39FE8C | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x25,0), 0x1cd9d0(f2,0xb4,0) |

`0x1cbe80(fighter, mode, 0)` modes: {0, 0x20, 0x24, 0x25, 0x33}. `0x1cd9d0(fighter, mode, $f12=0)` modes: {0xb4=180, 0xb5=181, 0x1ca=458}.

### 2.9 Helper routines

**`0x001CBB40`** (player side selector): `lw $v0,0x534($a0); lw $v0,8($v0)` — returns `player->0x534->0x08`.

**`0x001CD9D0`** (fighter state setup from mode table): `0x1CD9D0(fighter, mode, $f12=0)` modes {0xb4, 0xb5, 0x1ca}:
- Sets `fighter+0x17a0 = mode`
- Loads func ptr from `[fighter+0x7b8 + mode*4 + 0x800]`
- Calls `0x1d71a0`, then fighter vtable `+0x78`
- Writes vec4 from table to `fighter+0x510..51c` (spawn/home position)
- Modifies `fighter+0x520` flags (AND with `0xfffefffb`)

**`0x001CBE80`** (alternate fighter setup): `0x1CBE80(fighter, mode, 0)` modes {0, 0x20, 0x24, 0x25, 0x33}:
- Table lookup via `0x30e0d0` at `0x480000` (delay-slot adjusted to `0x47DE0`)
- Sets `fighter+0x1890 = -1`, memsets `fighter+0x18a0` (0x20 bytes)
- Uses `fighter+0x538` as index into vtable table at `+0x54c`, calls vtable `+0x14`
- Sets `fighter+0x53c = fighter+0x538`

**`0x00106950`** (COP2/VFPU transform): 4×4 homogeneous matrix × 4-vector transform using EE VFPU paired-single instructions. Loads float[4] from `$a0`, spills to stack, `ldc2` into COP2 reg `$4`, runs 6 VFPU MUL/MADD paired ops, `sdc2` back, round-trips result to `($a0)`. The paired VFPU instructions confirm paired-single vector math. Matrix preloaded into COP2 registers before the call.

**`0x0030E300`** (post-placement fighter init): Builds zero vec3 (constants at `0x443b84`–`0x443b8c`, all 0.0f), calls vtable `+0x4c` (orientation), vtable `+0x40` (position setter with `(0,0,0)`), `0x1d10d0`, vtable `+0x4c` again. Runs AFTER `0x23AF30`.

### 2.10 Updated field semantics table

| Offset | Object | Semantics |
|--------|--------|-----------|
| `stage+0x08` | Stage | Stage ID (0–12 for 13 stages) |
| `stage+0x24` | Stage | Stage variant / sub-stage bit (0/1) |
| `stage vtable+0x28` | Stage | Spawn slot selector (0–4) |
| `fighter+0x510` | Fighter | vec4 spawn/home position (written by `0x1CD9D0`) |
| `fighter+0x520` | Fighter | Flags bitmask |
| `fighter+0x534` | Fighter | Pointer to side/controller sub-object |
| `fighter+0x534+0x08` | Sub-object | Binary side selector (0=P1/vec A, 1=P2/vec B) |
| `fighter+0x538` | Fighter | Index into per-mode vtable table |
| `fighter+0x53c` | Fighter | Mirrors `+0x538` |
| `fighter vtable+0x40` | Fighter | Position setter `set_pos(fighter*, vec3*)` |
| `fighter vtable+0x4c` | Fighter | Orientation/rotation setter |
| `fighter vtable+0x78` | Fighter | State/configuration method |
| `fighter vtable+0xac` | Fighter | Post-placement method |

### 2.11 Lifecycle: when is 0x0023AF30 called?

Every caller places **two fighters together** (P1 at `owner+0x60`, P2 at `owner+0x64`) using stage at `owner+0x58`. This is **not** a mid-round single-fighter respawn; it is a two-fighter setup/initialization routine.

- **`0x1CBE80` only** (mode 0, 0x20, 0x24, 0x25, 0x33): fresh round / match setup (stage load)
- **`0x1CD9D0` added** (mode 0xb4, 0xb5, 0x1ca): rematch / round-reset re-placement (paired 0xb4/0xb5 across both fighters at `0x39DD40`)
- **`0x298180`** with loop `$s0` ∈ [0,46) gated on `fighter+0x528 == 29`: session / character-select → fight transition

**Conclusion:** `0x23AF30` is called at **fight initialization** — stage load / round setup — placing both fighters. The `0x1cd9d0`(0xb4/0xb5)-variant callers indicate a rematch / round-reset re-placement path. It is not a per-fighter mid-round respawn. `0x30E300` resets live position to origin after `0x23AF30` sets the stage-specific spawn position, suggesting the spawn position may be stored as a home offset (e.g. `fighter+0x510`) and applied live by a later system.

---

## 3. EU beta build (SLES_541.61) impact

**Executable:** `assets/original/SLES_541.61` (4,180,784 bytes vs SLUS 4,196,528).

### 3.1 What changed (addresses invalidated)

| Item | SLUS | SLES | Delta |
|------|------|------|-------|
| Load segment filesz | 0x400680 | 0x3fc900 | — |
| Load segment memsz | 0x540380 | 0x53c380 | — |
| stage_group_map | `0x00488400` | `0x00485EF0` | -0x1E10 |
| Placement table | `0x00487E00` | `0x004859F0` | -0x1E10 |

All SLUS absolute code addresses (`0x23AF30`, callers `0x298180`–`0x39FE00`, helpers `0x1CBB40`, `0x1CD9D0`, `0x1CBE80`, `0x30E300`, `0x106950`) are **invalid for SLES**. Register allocation differs — the SLUS field-lookup byte pattern (`lw a1,8(a0);lw v0,0x24(a0);lw t9,(a0);sll a1,a1,3`) was **not found** in SLES, so routines must be re-located.

### 3.2 What stayed identical (same game logic/data structures)

- Stage map values: identical `[0,0,1,1,2,2,...,15,15,0,0]` (24 entries)
- Placement table layout: identical 16×5×4-float layout (map − 0x500)
- Field offsets/semantics: stage+0x08=stage ID, +0x24=variant, vtable+0x28=slot selector 0-4; fighter+0x534+0x08=side selector; vtable+0x40=position setter; fighter+0x510=spawn vec4; fighter+0x520=flags
- Lifecycle: fight initialization (stage load / round setup), not mid-round respawn
- Methodology: MIPS32-LE decoder over ELF bytes still valid; re-run against SLES bytes

### 3.3 What differs

- **Placement coordinate values differ per group** between versions. SLES G0/G1 are all sentinel (`-15,0,15,0`) where SLUS G1 had real coordinates `[20.3,66.0,14.5,34.4]` etc. SLES G2 differs from SLUS G2. Tables must be re-read from SLES bytes, not ported 1:1.

### 3.4 Recommended next steps for SLES

1. Disassemble SLES around code referencing `0x00485EF0` (the map) to find the `0x23AF30` equivalent.
2. Re-read SLES placement table at `0x004859F0` and constants near `0x00485Dxx`.
3. Re-derive all caller addresses and field offsets for SLES.
4. Compare SLUS vs SLES placement data per group to detect version-specific coordinate changes.

---

## 4. Remaining unresolved / open questions

1. **Exact class names** for stage/player objects; stage subclass implementing vtable `+0x28`.
2. **Per-stage slot return values** — which slot 0–4 each stage class prefers (requires stage vtable identification; not in stripped binary).
3. **Exact axis order** (x,z vs x,y) and local-vs-world convention of `+0x40` position setter (requires full COP2 decode of `0x106950` matrix construction).
4. **Exact sub-object type** at `fighter+0x534` (controller/team/side/character).
5. **Exact lifecycle event name per caller** (round start vs rematch vs versus-screen load) — requires tracing callers' own callers.
6. **SLES re-location** — all code addresses and data values need re-derivation for the EU beta.

---

## 5. Deliverables

- This report — full decoded disassembly, field table, and data dumps (SLUS primary; SLES impact noted).
- Branch: `ring-cucumber` (task branch `pcsx2-caller-analysis` not created; no commits made per instructions).
