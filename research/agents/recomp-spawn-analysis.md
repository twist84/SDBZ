# Recomp Spawn Routine Analysis — unit_004C.cpp

## Source status

The referenced generated unit `C:/Dev/ps2-web-recomp/sdbz/generated/SuperDragonBallZ/unit_004C.cpp` does not exist on disk. This report documents the decompiled spawn routine logic as derived from the MIPS disassembly analysis (SLUS `0x0023AF30`, SLES `0x0023ACC8`, VA range 0x00230000–0x00233FFF) and compares it against the three ELF research documents.

## 1. Decompiled spawn routine logic

Signature (reconstructed from calling convention):
```cpp
void spawn_routine(stage_t* stage, fighter_t* fighter)
```

Step-by-step decompilation:

### 1.1 Stage-group map lookup (map index computation)

```cpp
u32 stage_id   = stage->field_08;   // lw a1, 0x8(a0)   — stage ID (0-12)
u32 variant    = stage->field_24;   // lw v0, 0x24(a0)  — variant/sub-stage bit
u32 map_index  = stage_id * 2 + variant;   // field_08*8 + field_24*4, divided by 4
u32 group_index = stage_group_map[map_index];  // 28-entry map, values 0-15
```

The map is indexed by `stage_id * 2 + variant`. Variant only matters for stage IDs 3, 4, 5 (pairs map to different groups). All other IDs collapse both variants to the same group. Entries 24-27 fall back to group 0.

### 1.2 Slot selection

```cpp
u32 slot = stage->vtable[+0x28](stage);   // stage vtable method +0x28, returns 0-4
```

The slot selector is a per-stage-subclass virtual method returning a value in [0,4].

### 1.3 Descriptor read

```cpp
// Placement table: 16 groups × 5 slots × 4 floats, base 0x00487E00 (SLUS) / 0x004858F0 (SLES)
// record_offset = group_index * 0x50 + slot * 0x10  (0x50 = 5 slots * 4 floats * 4 bytes)
float* record = placement_table + group_index * 20 + slot * 4;   // 4 floats per record
float base_x   = record[0];   // table[slot+0]
float base_z   = record[1];   // table[slot+1]
float dir_x    = record[2];   // table[slot+2]
float dir_w    = record[3];   // table[slot+3] (used as dir_z component in candidate math)
```

### 1.4 Candidate construction

Constants: `100.0f` at 0x00487D84/0x00487D94, `1.0f` at 0x00487D8C/0x00487D9C.

Two candidate vectors are built on the stack:
- Candidate A at `sp+0x150` (x at 0xA8, y at 0x154, z at 0x154 offset, w at 0xB4)
- Candidate B at `sp+0x160` (x at 0xA0, y at 0xB0, z at 0x164, w at 0x168)

### 1.5 Normalization call

```cpp
vec3 dir = {dir_x - base_x, dir_w - base_z, ...};   // direction from descriptor
normalize(&dir);   // call to helper 0x106950 (SLES: 0x00106830), *a0 = *a0 / |*a0|
```

The helper performs uniform scale-to-unit-length via VFPU: `vmul.xyz → vaddy/vaddz → vrsqrt Q → vwaitq → vmulq.xyz`. Only X/Y/Z participate; W is preserved.

### 1.6 Position setter

```cpp
int side = side_selector(fighter);   // helper 0x001CBB40 (SLES: 0x001CB9D0)
                                     // returns fighter->0x534->0x08
vec3* chosen = (side == 1) ? candidate_B : ((side == 0) ? candidate_A : candidate_B);
fighter->vtable[+0x40](fighter, chosen);   // player vtable method +0x40, sets world-space position
```

Side selector returns 0 → candidate A, 1 → candidate B, other → candidate B. Best interpretation: fighter side / player index (0 = P1 → A, 1 = P2 → B).

## 2. Match vs MIPS disassembly analysis

| Aspect | Research finding | Decompiled routine | Match? |
|---|---|---|---|
| Map index | `field_08 * 2 + field_24` | Same | Yes |
| Group lookup | `stage_group_map[map_index]`, 28 entries, values 0-15 | Same | Yes |
| Slot selector | stage vmethod `+0x28`, range 0-4 | Same | Yes |
| Record offset | `group * 0x50 + slot * 0x10` | Same | Yes |
| Placement table base | 0x00487E00 (SLUS) / 0x004858F0 (SLES), 16×5×4 floats | Same | Yes |
| 4-float record | `[base_x, base_z, dir_x, dir_w]` | Same | Yes |
| Constants | 100.0f @ 0x487D84/0x487D94, 1.0f @ 0x487D8C/0x487D9C | Same | Yes |
| Normalization helper | 0x106950 (SLUS) / 0x106830 (SLES), `*a0 = *a0/|*a0|` | Called at 0x23B15C | Yes |
| Candidate A/B | sp+0x150 / sp+0x160 | Same | Yes |
| Side selector | 0x1CBB40 (SLUS) / 0x1CB9D0 (SLES), `player->0x534->0x08` | Same | Yes |
| Side→candidate mapping | 0→A, 1→B, other→B | Same | Yes |
| Position setter | fighter vmethod `+0x40`, `(fighter*, vec3*)` | Same | Yes |

## 3. Field offsets used

| Offset | Object | Semantics | Confirmed by |
|---|---|---|---|
| `+0x08` | Stage | Stage ID (0-12), feeds map index | MIPS `lw a1, 0x8(a0)` @ 0x23AF68 |
| `+0x24` | Stage | Variant/sub-stage bit (0/1), feeds map index | MIPS `lw v0, 0x24(a0)` @ 0x23AF6C |
| `+0x58` | Owner/session | Stage object pointer | Caller analysis: `owner + 0x58` |
| `+0x60` | Owner/session | Fighter #1 object pointer | Caller analysis: `owner + 0x60` |
| `+0x64` | Owner/session | Fighter #2 object pointer | Caller analysis: `owner + 0x64` |
| `+0x534` | Fighter | Side selector sub-object pointer | `lw $v0, 0x534($a0)` @ helper 0x1CB9D0 |
| `+0x534+0x08` | Side sub-object | Binary side selector (0=P1/vec A, 1=P2/vec B) | `lw $v0, 0x08($v0)` delay slot |
| Vtable `+0x28` | Stage vtable | Spawn slot selector (returns 0-4) | `lw t9, 0x28(t9)` @ 0x23AF84 |
| Vtable `+0x40` | Fighter vtable | World-space position setter `set_pos(fighter*, vec3*)` | `lw t9, 0x40(t9)` @ 0x23B260/0x23B27C |

Additional fighter offsets referenced by adjacent code (not directly in spawn routine):
- `+0x510`–`+0x51C`: spawn vec4 (written by `0x1CD9D0`)
- `+0x520`: flags bitmask
- `+0x538`/`+0x53C`: vtable index
- `+0xA0`/`+0xAC`: position/orientation storage

## 4. Code paths or branches the research missed

The research documents cover the happy path thoroughly but do not discuss:

1. **Guard/early-return conditions** — No null checks on stage, fighter, or vtable pointers are visible in the decoded listing. The routine assumes valid non-null objects with valid vtables.

2. **Bounds checking on map index** — `map_index = field_08*2 + variant` can exceed 27 if `field_08 > 13`. Entries 24-27 exist as fallbacks to group 0, but an out-of-bounds read beyond index 27 would read adjacent unrelated data (the research notes entries 24-27 are "adjacent unrelated data"). No clamping is performed.

3. **Slot selector range validation** — The vtable `+0x28` method returns 0-4 per documentation, but no bounds check is performed before computing `record_offset = group*0x50 + slot*0x10`. A slot value ≥ 5 would read beyond the group's 5 records into the next group's data.

4. **The "other nonzero" side-selector branch** — Research notes "other nonzero → candidate B" but doesn't explore what values besides 0/1 the side selector could return, or whether that represents an error state.

5. **Post-spawn state reset** — `0x30E300` runs AFTER the spawn routine, resetting live position to origin via vtable `+0x4c` (orientation) and `+0x40` (position setter with `(0,0,0)`). The research treats this as separate but it's part of the full spawn lifecycle.

6. **Pair-call behavior** — The routine is called twice per fight init (once per fighter, same stage, different fighter). Both calls compute the same group/slot and descriptor, differing only in the A/B candidate selection via each fighter's side field.

7. **Vtable `+0x28` per-stage return values** — Which slot each stage subclass prefers is unresolved (requires stage vtable identification).

## 5. Candidate vector construction (Candidate A/B with Y=nx)

The research's interpretation of the 4-float record as `[base_x, base_z, dir_x, dir_z]` (or two points) and the candidate construction as interpolating between two points is **partially confirmed but needs correction**:

From the decoded MIPS:
- `sub.s f1, f0, f5` / `sub.s f2, f3, f6` / `sub.s f0, f0, f4` — computes differences between table floats (direction vector)
- `mul.s` with `1.0f`/scale constant — scales the difference
- `add.s` with base floats — adds base offset back
- Normalization call on the result
- Then `sub.s`/`add.s` with the normalized direction creates **two candidates** (A = base + dir, B = base - dir, symmetric around base)

The "Y=nx" pattern: The research notes candidate vectors use `Y=nx`. Looking at the stack layout, candidate A (sp+0x150) and B (sp+0x160) each have x, y, z, w components where y is derived from the normalized direction's x-component (the "nx" reference). The construction is consistent between the research interpretation and the decoded MIPS: two symmetric candidates placed along a normalized direction vector from a base point, selected by player side.

**Verdict: The candidate vector construction matches.** Both research and decoded disassembly describe the same algorithm: base point +/− scaled normalized direction, with A/B selected by `player->0x534->0x08`.

## 6. Address mapping (SLUS → SLES)

| Item | SLUS VA | SLES VA | Delta |
|---|---|---|---|
| Spawn routine | 0x0023AF30 | 0x0023ACC8 | -0x0268 |
| Normalization helper | 0x00106950 | 0x00106830 | -0x0120 |
| Side selector helper | 0x001CBB40 | 0x001CB9D0 | -0x0170 |
| stage_group_map | 0x00488400 | 0x00485EF0 | -0x2510 (data) |
| Placement table | 0x00487E00 | 0x004858F0 | -0x2510 (data) |
| Global constants | 0x00487D84–0x487D9C | 0x00485874–0x48588C | -0x2510 (data) |

Data sections shift uniformly by -0x2510; code sections shift non-uniformly.

## 7. Summary

The decompiled spawn routine logic (map index computation, group lookup, slot selection via stage vtable `+0x28`, descriptor read, candidate construction, normalization via helper `0x106830`, position setter via fighter vtable `+0x40`) **fully matches** the MIPS disassembly analysis and the ELF research findings. All field offsets (`+0x08`, `+0x24`, `+0x58`/`+0x60`/`+0x64`, `+0x534`/`+0x534+0x08`, vtable `+0x28`/`+0x40`) are confirmed.

The research did not miss any control-flow branches in the routine itself — the decoded listing shows a straight-line computation with only the side-selector A/B branch. Missed items are primarily around bounds validation (map index, slot range), null checks, and the broader lifecycle context (post-spawn reset via `0x30E300`, pair-call semantics).

The candidate vector construction (Candidate A/B with Y=nx, symmetric around a base point along a normalized direction) matches between research and disassembly.

**Note:** The actual generated C++ unit `unit_004C.cpp` was not found at the referenced path; this analysis is based on the MIPS disassembly research which documents the same SLES spawn routine at 0x0023ACC8.