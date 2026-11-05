# Super Dragon Ball Z — ELF Spawn Research Report

## 1. Overview

Research into the PlayStation 2 executable's player placement routine, conducted on `SLUS_214.42` (US version). The goal was to recover the original stage-to-player placement logic for reproduction in the native C++ rebuild.

## 2. Input Executable

- File: `SLUS_214(2).42` (also at `assets/original/SLUS_214.42`)
- Format: ELF32, little-endian, MIPS R3000/MIPS III
- Statically linked, not stripped
- Size: 4,196,528 bytes
- ELF entry: `0x00100008`
- Load segment: VA `0x00100000`, file offset `0x80`, size `0x400680`
- Address mapping: `file_offset = va - 0x00100000 + 0x80`

## 3. Primary Routine: 0x0023AF30

- Range: `0x0023AF30`–`0x0023B2B0` (928 bytes)
- Arguments: `$a0` = stage/session object, `$a1` = fighter/player object (copied to `$s5`)
- Stack frame: `0x170` bytes

### Calling Convention
- `$a0` = stage object (fields at `+0x08`, `+0x24`, vtable at `+0x00`)
- `$a1` = fighter object (copied to `$s5`, used as player for vtable `+0x40` setter)

### Stage Group Lookup
```c
u32 map_index = stage->field_08 * 2 + stage->field_24;
u32 group_index = stage_group_map[map_index];  // at ELF 0x00488400
```

### Slot Selection
```c
u32 slot_selector = stage->vtable[+0x28]();  // returns 0-4
u32 record_offset = group_index * 0x50 + slot_selector * 0x10;
// record at ELF 0x00487E00 + record_offset (4 floats)
```

### Candidate Vector Construction
- Reads 4 floats from placement table at record offset
- Builds direction vector: `dx = table[2] - table[0]`, `dz = table[3] - table[1]`
- Normalizes via helper 0x00106950: `mag = sqrt(dx² + dz²)`, `nx = dx/mag`, `nz = dz/mag`
- Constructs two candidate positions:
  - **Candidate A** ($sp+0x150): `{mid_x + 15.5*nx, nx, t1 + 15.5*nz, 1.0}`
  - **Candidate B** ($sp+0x160): `{mid_x - 14.5*nx, nx, t1 - 14.5*nz, 1.0}`
  - where `mid_x = (t0 + t2) / 2`

### Side Selection
```c
u32 side = helper_0x1CBB40(player);  // returns player->0x534->0x08
// side == 1 → Candidate B (sp+0x160)
// side == 0 → Candidate A (sp+0x150)
// other → Candidate B
```

### Position Setter
```c
player->vtable[+0x40](player, selected_vector);  // 3-float position setter
```

## 4. Helper 0x00106950 — Vector Normalization (FULLY DECODED)

Previously undecoded due to COP2/MIPS-3D instructions. Now fully decoded:

```asm
0x00106950: addiu sp, sp, -0x10
0x00106954: lwc1  f3, ($a0)        ; load input[0]
0x00106958: addiu v1, sp, 0
0x0010695C: lwc1  f2, 4($a0)       ; load input[1]
0x00106960: lwc1  f1, 8($a0)       ; load input[2]
0x00106964: lwc1  f0, 0xC($a0)     ; load input[3]
0x00106968-74: swc1 f3,f2,f1,f0, ($sp)  ; store to stack
0x00106978: ldc2  vf04, ($v1)       ; load into COP2
0x0010697C: vmul.xyz vf05, vf04, vf04z   ; square xz
0x00106980: vaddy.x vf05, vf05, vf05x    ; accumulate x²
0x00106984: vaddz.x vf05, vf05, vf05x    ; accumulate z²
0x00106988: vrsqrt Q, vf00w, vf05x       ; Q = 1/sqrt(dx²+dz²)
0x0010698C: vwaitq
0x00106990: vmulq.xyz vf04, vf04, Q      ; scale to unit
0x00106994: sdc2  vf04, ($v1)            ; store back
0x00106998-BC: lwc1/swc1 f3,f2,f1,f0  ; reload and write to *a0
0x001069B8: jr $ra
0x001069BC: addiu sp, sp, 0x10
```

**Net effect: `*a0 = *a0 / |*a0|` (2D xz-plane normalization). NOT a matrix/rotation.**

## 5. Helper 0x001CBB40 — Nested Field Accessor

```asm
0x001CBB40: lw $v0, 0x534($a0)
0x001CBB44: jr $ra
0x001CBB48: lw $v0, 0x08($v0)
```

Returns `player->0x534->0x08`. Binary side/player-index selector (P1 vs P2). Not a collision query.

## 6. Field Semantics

| Field | Location | Meaning |
|-------|----------|---------|
| stage+0x08 | Stage object | Stage ID (0-12 for 13 stages) |
| stage+0x24 | Stage object | Variant/sub-stage bit (0/1) |
| stage vtable +0x28 | Stage object | Spawn-slot selector (0-4) |
| player+0x534->+0x08 | Player object | Side/player-index (P1 vs P2) |
| player vtable +0x40 | Player object | World-space 3-float position setter |

## 7. Placement Table Data (from SLUS_214.42)

### Table (ELF VA 0x00487E00, file offset 0x387E80)
- 16 groups × 5 slots × 4 floats = 320 floats (0x500 bytes)
- Each slot: [x, y, z, w] four-float descriptor

### Stage-Group Map (ELF VA 0x00488400, file offset 0x388480)
24 meaningful entries: `[0,0,1,1,2,2,3,4,5,6,7,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,0,0]`

### Globals
- 0x00487D84: 100.0f
- 0x00487D8C: 1.0f
- 0x00487D94: 100.0f
- 0x00487D9C: 1.0f

## 8. Stage-to-Group Mapping

| Stage | field_08 | Var0→Group | Var1→Group | Data |
|-------|----------|------------|------------|------|
| s00 | 0 | 0 (sentinel) | 0 (sentinel) | none |
| s01 | 1 | 1 (real) | 1 (real) | real |
| s02 | 2 | 2 (real) | 2 (real) | real |
| s03 | 3 | 3 (sentinel) | 4 (sentinel) | none |
| s04 | 4 | 5 (real) | 6 (real) | real |
| s05 | 5 | 7 (real) | 8 (real) | real |
| s06 | 6 | 9 (sentinel) | 9 (sentinel) | none |
| s07 | 7 | 10 (sentinel) | 10 (sentinel) | none |
| s08 | 8 | 11 (real) | 11 (real) | real |
| s09 | 9 | 12 (real) | 12 (real) | real |
| s10 | 10 | 13 (real) | 13 (real) | real |
| s11 | 11 | 14 (real) | 14 (real) | real |
| s12 | 12 | 15 (sentinel) | 15 (sentinel) | none |

Sentinel groups: 0, 3, 4, 9, 10, 15 (no real placement data).
Real data groups: 1, 2, 5, 6, 7, 8, 11, 12, 13, 14.

## 9. Coordinate Convention

- 4-float slot = [x, y, z, w]; float[1] = vertical Y
- Y-up, right-handed, ground at y=0, w≈1.0 (padding)
- Evidence: group 13 slots 1/2 fighters at y=0, w=0, opposite corners [-165,0,-135]/[135,0,165]

## 10. Callers (8 total)

All follow pattern: stage=session at owner+0x58, fighter1 at owner+0x60, fighter2 at owner+0x64. Two 0x23AF30 calls then 0x30E300 per fighter.

| Caller | Calls | Config | Lifecycle |
|--------|-------|--------|-----------|
| 0x00298180 | 0x29820C, 0x298218 | loop $s0 0-45 gated on fighter+0x528==29 | fight init |
| 0x00392D80 | 0x392DF4, 0x392E00 | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x33,0) | fight init |
| 0x003956A0 | 0x395728, 0x395734 | 0x1cbe80(f1,0x20,0), 0x1cbe80(f2,0x24,0), 0x1cd9d0(f2,0x1ca,0) | fight init |
| 0x00398910 | 0x398978, 0x398984 | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x25,0), 0x1cd9d0(f2,0xb4,0) | fight init / rematch |
| 0x00399640 | 0x3996F0, 0x3996FC | same as 0x3956A0 | fight init |
| 0x0039B550 | 0x39B59C, 0x39B5A8 | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x33,0) | fight init |
| 0x0039DD40 | 0x39DDB4, 0x39DDC0 | 0x1cbe80(f1,0x25,0), 0x1cbe80(f2,0x25,0), 0x1cd9d0(f1,0xb4), 0x1cd9d0(f2,0xb5) | rematch / round-reset |
| 0x0039FE00 | 0x39FE7C, 0x39FE8C | 0x1cbe80(f1,0,0), 0x1cbe80(f2,0x25,0), 0x1cd9d0(f2,0xb4,0) | fight init / rematch |

Lifecycle: two-fighter placement at fight initialization (stage load/round setup), NOT mid-round respawn.

## 11. EU Beta Build Impact

**All findings are specific to SLUS_214.42 (US version).** If targeting EU beta (SLES_541.61):

1. **ELF ADDRESSES**: All virtual addresses (routine, helper, table, map, globals, callers) are US-specific. Must re-verify against EU beta ELF.
2. **TABLE DATA**: The 16×5×4 float table and 24-entry stage-group map are extracted from US ELF. Values may differ.
3. **CALLER ADDRESSES**: All 8 caller addresses are US-specific.
4. **STAGE IDS**: 13-stage mapping correlated against US ELF; EU beta may differ.
5. **FIELD OFFSETS** (+0x08, +0x24, +0x534, vtable +0x28/+0x40): More likely stable (class layout), but must verify.
6. **REFERENCE MODULE**: `source/core/original_spawn.{hpp,cpp}` contains hardcoded US table data. Must re-extract from EU beta ELF. Structural code (arithmetic, normalization) is likely portable.

**Recommendation**: Re-run PCSX2 disassembly pipeline against EU beta ELF to confirm/update all addresses and table data before wiring into gameplay.

## 12. Implementation Status

### Completed
- Reference module: `source/core/original_spawn.{hpp,cpp}` with full transform implemented
- Tests: `tests/test_original_spawn.cpp` with transform validation (all 6 CTests pass under -Werror)
- Research log: `SLUS_spawn_research_log.md` updated with all findings
- Build: clean, all tests pass

### Not Done
- Wiring into gameplay (`place_player_on_stage` in `sdl_main.cpp`) — awaiting validation
- Slot selection per stage (stage vtable +0x28 return domain)
- field_24 variant semantics beyond 0/1 bit
- Validation against actual stage geometry

## 13. Open Questions
1. What does stage vtable +0x28 return for each stage? (slot 0-4 domain)
2. What is the semantic meaning of field_24 (variant bit)?
3. Are the candidate positions (with y=nx) used directly or adjusted by the game engine?
4. How do sentinel groups (0,3,4,9,10,15) handle spawning? (No real placement data)
5. Does the EU beta have the same structure or different addresses/data?

## 14. Artifacts
- `SLUS_spawn_research_log.md` (updated research log)
- `source/core/original_spawn.hpp` (reference module header)
- `source/core/original_spawn.cpp` (reference module implementation with embedded US table data)
- `tests/test_original_spawn.cpp` (validation tests)
- `.kilo/worktrees/lapis-hill/` (disassembly files: routine_disasm.txt, helper_disasm.txt, raw_instrs.txt)
- `.kilo/worktrees/bygone-hubcap/` (stage correlation: placement_report.txt, STAGE_CORRELATION_REPORT.md)
- `.kilo/worktrees/ring-cucumber/` (caller analysis report)
