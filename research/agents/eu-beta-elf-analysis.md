# Super Dragon Ball Z — SLES_541.61 (EU beta) ELF Re-disassembly

## Input executable

- File: `C:\Dev\SuperDragonBallZ\assets\original\SLES_541.61`
- Format: ELF32, little-endian
- Machine: MIPS R3000 / MIPS III (PS2 Emotion Engine, runs in kseg0 at 0x8xxxxxxx)
- Linked: statically linked, not stripped
- File size: 4,180,784 bytes
- ELF entry point: `0x00100008`
- Load segment: PT_LOAD vaddr `0x00100000`, file offset `0x80`, file size `0x003FC900`
- Address translation: `file_offset = virtual_address - 0x100000 + 0x80`
- kseg0 runtime mapping: add `0x80000000` to ELF vaddr for execution

## Re-derived addresses

| Symbol | SLUS_214.42 | SLES_541.61 (vaddr) | SLES_541.61 (file offset) | Delta (vaddr) | Notes |
|---|---|---|---|---|---|
| Spawn routine | `0x0023AF30` | `0x0023ACC8` | `0x0013AD48` | `-0x00000268` | No direct `jal` callers; entered via fall-through/alt entries |
| Pre-spawn object resolver | — | `0x0023A430` | `0x0013A4B0` | n/a | Resolves stage+fighters from owner/session object |
| Spawn sub-routine entry | — | `0x0023B140` | `0x0013B1C0` | n/a | Main entry, called by 14 callers |
| Spawn alternate entry | — | `0x0023B0E0` | `0x0013B160` | n/a | Called by 1 caller (0x2AD2D4) |
| Spawn alternate entry | — | `0x0023B020` | `0x0013B0A0` | n/a | Called by 1 caller (0x3EEFAC) |
| Vector normalization helper | `0x00106950` | `0x00106830` | `0x000068B0` | `-0x00000120` | Standalone function |
| Side selector helper | `0x001CBB40` | `0x001CB9D0` | `0x000CBA50` | `-0x00000170` | player->0x534->0x08 |
| Fighter object init/zeroing | — | `0x001CB4C0` | `0x000CB540` | n/a | Clears fighter fields including +0x538/+0x53C |
| Fighter spawn vec4 store | — | `0x001CB564` | `0x000CB5E4` | n/a | Stores vec4 at +0x510–+0x51C |
| Fighter side selector getter | — | `0x001CB5A0` | `0x000CB620` | n/a | Loads +0x534 side selector sub-object |
| Fighter flags accessor | — | `0x001CB5E8` | `0x000CB668` | n/a | Loads/stores +0x520 flags |
| Fighter state machine core | — | `0x0022DED0` | `0x0022DF50` | n/a | State transitions, side selector, flag checks |
| Fighter position/orientation update | — | `0x001CB7C0` | `0x000CB840` | n/a | COP2 vector ops, vtable+0x78 state config |
| stage_group_map | `0x00488400` | `0x00485EF0` | `0x00385F70` | `-0x00002510` | 28 entries |
| Placement table | `0x00487E00` | `0x004858F0` | `0x00385970` | `-0x00002510` | 16 groups × 5 slots × 4 floats |
| Global constant 100.0f (#1) | `0x00487D84` | `0x00485874` | `0x003858F4` | `-0x00002510` | |
| Global constant 1.0f (#1) | `0x00487D8C` | `0x0048587C` | `0x003858FC` | `-0x00002510` | |
| Global constant 100.0f (#2) | `0x00487D94` | `0x00485884` | `0x00385904` | `-0x00002510` | |
| Global constant 1.0f (#2) | `0x00487D9C` | `0x0048588C` | `0x0038590C` | `-0x00002510` | |

**Code addresses do NOT follow the uniform -0x2510 data shift.** Data sections shift by exactly -0x2510. Code addresses shift by non-uniform amounts because the EU beta was rebuilt with different code layout:
- Spawn routine: -0x0268
- Normalization helper: -0x0120
- Side selector: -0x0170

## Fighter object lifecycle

### Fighter object construction/allocation

**Object zeroing at `0x001CB4C0`–`0x001CB51C`**: Clears fighter fields on allocation:
```
0x001CB4EC: sw $zero, 0x544($a0)
0x001CB4F0: sw $zero, 0x548($a0)
0x001CB4F4: sw $zero, 0x1CC4($a0)
0x001CB4F8: sw $zero, 0x538($a0)    ; vtable index cleared
0x001CB4FC: sw $zero, 0x53C($a0)    ; vtable index cleared
0x001CB500: sw $zero, 0x540($a0)
0x001CB504: sw $zero, 0x714($a0)
0x001CB508: sw $zero, 0x1848($a0)
0x001CB50C: sw $zero, 0x184C($a0)
0x001CB510: sw $zero, 0x1850($a0)
0x001CB514: sw $zero, 0x1854($a0)
0x001CB518: sw $zero, 0x1858($a0)
0x001CB51C: sw $zero, 0x698($a0)
```
Additional zeroing at `0x001CB3F4`: `sw $zero, 0x53C($a0)`.

**Fighter construction cluster at `0x0022D600`–`0x0022DB30`**: Multiple sub-functions handling different fighter init phases (side selection, position setup, vtable calls, state configuration).

### Fighter field population

| Field offset | Purpose | SLES address (store/load) | Notes |
|---|---|---|---|
| `+0x510` | Spawn vec4 X | `0x001CB564` (`swc1`) | Part of spawn position vector |
| `+0x514` | Spawn vec4 Y | `0x001CB568` (`swc1`) | Part of spawn position vector |
| `+0x518` | Spawn vec4 Z | `0x001CB56C` (`swc1`) | Part of spawn position vector |
| `+0x51C` | Spawn vec4 W | `0x001CB574` (`swc1`) | Part of spawn position vector |
| `+0x520` | Flags | `0x001CB5E8` (ld), `0x001CB5F8` (st), `0x001CB618` (ld), `0x001CB624` (st), `0x001CB2B0` (ld), `0x0022DD80` (ld), `0x0022E404` (ld) | Bit flags for fighter state |
| `+0x534` | Side selector sub-object ptr | `0x001CB590` (ld), `0x001CB5A0` (ld), `0x001CB2B0` (ld) | Returns player->0x534 |
| `+0x538` | Vtable index | `0x001CB4F8` (st zero) | Cleared on init |
| `+0x53C` | Vtable index | `0x001CB4FC` (st zero), `0x001CB3F4` (st zero) | Cleared on init |

Spawn vec4 loaded from data table at `0x449F70`–`0x449F7C` (group data), stored to fighter at `+0x510`–`+0x51C`.

### Side selector sub-object

**Helper at `0x001CB9D0`** (SLUS `0x1CBB40`):
```
0x001CB9D0: lw   $v0, 0x534($a0)   ; load player->0x534 (side_data ptr)
0x001CB9D4: jr   $ra                ; return
0x001CB9D8: lw   $v0, 0x08($v0)    ; delay slot: side_data->0x08 = side value
```
Returns `player->0x534->0x08` — the P1/P2 side selector (0 or 1).

**Getter functions**:
- `0x001CB5A0`: `lw $v0, 0x534($a0); jr $ra` — direct side selector sub-object accessor
- `0x001CB594`: `lw $a0, 0x534($a0)` — loads side selector sub-object before further processing

56 direct `jal` callers of the side selector found across the ELF, including inside the spawn routine at `0x0023AF94` and throughout fighter state machine code.

### Fighter state machine transitions

**State machine core at `0x0022DED0`–`0x0022DFEC`**:
```
0x0022DED0: jal side_selector        ; determine P1/P2 side
0x0022DED8: lhu $v1, 0x1A($v0)      ; load state/phase halfword
0x0022DEDC: ori $v1, $v1, 0x140     ; combine with phase flag
0x0022DEE0: bne $v1, $zero, +0x08    ; branch if non-zero
0x0022DEE4: nop
0x0022DEE8: sb $zero, 0x04($v0)     ; clear flag at +0x04
0x0022DEEC: lbu $v1, 0x34($v0)      ; load state byte at +0x34
0x0022DEF0: slti $at, $v1, 0x3C     ; compare against threshold 60
0x0022DEF4: bne $at, $zero, +0xC4
0x0022DEF8: li $v0, 0x3C            ; set to 60 if below threshold
0x0022DEFC: sb $v0, 0x04($v0)       ; store clamped value at +0x04
```

**State byte switch/case patterns**:
- At `0x0022DC1C`–`0x0022DC58`: Switch on byte values `0x1E`, `0x15`, `0x14`, `0x0A`, `0x01`
- At `0x0022E358`–`0x0022E398`: Switch on byte values `0x0E`, `0x0D`, `0x0C`, `0x0B`, `0x0A`, `0x01`
- At `0x0022E404`–`0x0022E448`: Flags check at +0x520 with state transitions

**Position/orientation update at `0x001CB7C0`–`0x001CB92C`**:
```
0x001CB7E4: lw   $t9, 0($t9)         ; vtable load
0x001CB7E8: lw   $t9, 0x78($t9)      ; vtable +0x78 (state config method)
0x001CB7EC: jalr $t9                 ; call state config
0x001CB7F4: lwc1 $f3, 0x00($a0)      ; load position X
0x001CB7F8: lwc1 $f3, 0xA0($a0)      ; load position Y from +0xA0
0x001CB800: swc1 $f2, 0xA0($a0)      ; store position Y at +0xA0
0x001CB804: swc1 $f1, 0xA4($a0)      ; store position Z at +0xA4
0x001CB808: swc1 $f0, 0xA8($a0)      ; store position W at +0xA8
0x001CB8A4: swc1 $f0, 0xAC($a0)      ; store post-placement value at +0xAC
```
COP2 MIPS-3D vector operations for position/orientation math throughout this range.

### Vtable method references

| Vtable offset | Method | SLES call sites | Notes |
|---|---|---|---|
| `+0x40` | Position setter | `0x0023AFC0`, `0x0023AFDC`, `0x0023B0C0` | Called inside spawn routine; sets world-space position |
| `+0x4C` | Orientation | `0x0022DF48` (`lw $t9, 0x4C($t9)`) | Sets fighter orientation |
| `+0x78` | State config | `0x0022DE14`, `0x001CB7E8` (`lw $t9, 0x78($t9)`) | Returns state configuration data |
| `+0xAC` | Post-placement | `0x001CB8FC` (`swc1 $f0, 0xAC($a0)`) | Stores post-placement result |
| `+0x28` | Spawn slot selector | `0x0022DF34` (`lw $t9, 0x28($t9)`) | Selects spawn slot |

### Fighter creation/destruction per match

**Creation paths (14 callers targeting `0x0023B140`)**:
- Fight init: callers at `0x0023A34C`, `0x00251F50`, `0x00262AC8`, `0x002876C0`, `0x00288A04`, `0x003EEFAC` (via `0x23B020`)
- Rematch: callers at `0x0025297C`, `0x00270798`, `0x00296E7C`
- Round reset: callers at `0x00259B24`, `0x0029AAA8`
- Stage transition: callers at `0x0025A3A4`, `0x0029F2D8`
- Versus screen: caller at `0x002AD2D4` (via lightweight `0x23B0E0` entry)

**Common creation pattern** (14 of 16 callers):
```
jal 0x23A430    ; pre-spawn: resolve stage+fighter objects from owner/session
jal 0x23B140    ; spawn sub-routine (main entry)
```

**Pre-spawn object resolver at `0x0023A430`**:
- Stage object: resolved from owner+4 via stage vtable+0xB0 call
- Fighter 1 (+0x60): resolved from owner via owner vtable+0x98 call
- Fighter 2 (+0x64): resolved from owner via owner vtable+0xA4 call
- Owner/session object stored at owner+0x2C

## COP2 normalization helper

Located at vaddr `0x00106830` (file offset `0x000068B0`). Confirmed standalone normalization function:

```
0x0010685C: vmul.xyz vf05,vf04,vf04    ; square (component-wise)
0x00106860: vaddy vf05,vf05,vf05       ; sum of squares (y)
0x00106864: vaddz vf05,vf05,vf05       ; sum of squares (z)
0x00106868: vrsqrt Q,vf00w,vf05x       ; Q = 1/|v|
0x0010686C: vwaitq                     ; wait for Q
0x00106870: vmulq.xyz vf04,vf04,Q      ; scale to unit length
```

Semantics: `*a0 = *a0 / |*a0|` — uniform unit-length vector normalization. Identical to SLUS `0x106950`. Four COP2 sequences found in `.text`; match #1 at `0x10685C` is inside the standalone helper; matches at `0x107D98`, `0x107DFC`, `0x107EC8` are inlined copies.

## stage_group_map (file offset 0x00385F70)

28 entries confirmed identical to SLUS:

| Index | 00 | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Value | 0 | 0 | 1 | 1 | 2 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 9 | 10 | 10 | 11 | 11 | 12 | 12 | 13 | 13 | 14 | 14 | 15 | 15 | 0 | 0 |

Entries 0–23 select placement group 0–15. Entries 24–27 are adjacent unrelated data.

## Placement table (file offset 0x00385970)

16 groups × 5 slots × 4 floats = 320 floats (0x500 bytes). All values identical to SLUS. Table ends at file offset `0x385E70`; 0x100-byte gap before map at `0x385F70`.

## Data comparison: SLES vs SLUS

### Map entries — IDENTICAL
All 28 `stage_group_map` entries match SLUS exactly.

### Placement table — IDENTICAL
All 16 groups × 5 slots × 4 floats match SLUS exactly.

### Global constants — IDENTICAL
`100.0f`, `1.0f`, `0.0f` pairs match SLUS.

### Fighter field layout — IDENTICAL
All fighter field offsets (+0x510, +0x520, +0x534, +0x538, +0x53C, +0xA0, +0xAC) match SLUS semantics.

### Address shift
- Data sections: uniform `-0x2510` shift
- Code sections: non-uniform shifts (spawn: -0x0268, helper: -0x0120, side selector: -0x0170)
- No data content differs between builds

### Semantics — UNCHANGED
- Stage field `+0x08` = stage ID (0–12)
- Stage field `+0x24` = variant/sub-stage bit (0/1)
- Map index = `field_08 * 2 + field_24`
- Table offset = `group * 0x50 + slot * 0x10`
- Four-float slot = `[x, y, z, w]`, Y-up
- Owner/session object: stage at +0x58, fighters at +0x60/+0x64
- Player vtable `+0x40` = world-space position setter
- Side selector: `player->0x534->0x08`
- Fighter spawn vec4: `+0x510` to `+0x51C`
- Fighter flags: `+0x520`
- Fighter vtable index: `+0x538` / `+0x53C`
- Fighter state config: vtable `+0x78`
- Fighter post-placement: `+0xAC`

## Not covered

**MOT2 sampling**: Not located. No pattern-scan or address search was performed for MOT2 (motion) sampling code or data structures. This would require additional reverse-engineering effort beyond the spawn/fighter lifecycle scope.

**Character selection**: Not located. No code was identified or analyzed for character selection UI/logic. This subsystem is separate from the spawn and fighter lifecycle code analyzed here.

**Reason**: The research scope focused on the spawn routine, fighter object lifecycle, position setter, side selector helper, and stage lifecycle caller sites. MOT2 sampling and character selection subsystems were not within the assigned scope and no time/resources were allocated to locate them.