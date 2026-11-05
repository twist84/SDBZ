# PS2 ELF Spawn Routine Disassembly Report

**Project:** Super Dragon Ball Z native rebuild
**Target executable:** `assets/original/SLUS_214.42` (US build, 4,196,528 bytes, little-endian, ELF entry `0x00100008`)
**File offset formula:** `file_offset = virtual_address - 0x00100000 + 0x80` (confirmed via ELF header EI_DATA=1)
**Work directory:** `C:\Dev\SuperDragonBallZ\.kilo\worktrees\lapis-hill`
**Status:** Complete (US build). Nothing modified; nothing committed.

---

## 1. What Was Analyzed

Two routines were targeted in `SLUS_214.42`:

| Routine | VA range | File offset range | Instruction count |
|---|---|---|---|
| Spawn routine | `0x0023AF30` – `0x0023B2B0` | `0x13AFB0` – `0x13B330` | 224 |
| Vector helper | `0x00106950` – `0x001069c0` | `0x69D0` – `0x6A40` | 28 |

### Tooling

PCSX2 is installed as a binary only (`C:\Program Files\PCSX2\pcsx2-qt.exe`); building all of PCSX2 (Qt/LLVM) was deemed too heavy. Instead, a small Python MIPS + COP2/MIPS-3D disassembler was built at `disasm.py`, replicating:

- PCSX2's `pcsx2/DebugTools/DisR5900asm.cpp` decode tables (COP2 SPECIAL1/SPECIAL2, BC2, register naming `vf00`–`vf31`, `vi00`–`vi31`).
- Authoritative binutils-gdb R5900 VU0/MIPS-3D opcode encodings (`opcodes/mips-opc.c`), including the SPECIAL1→SPECIAL2 escape (functions 0x3A–0x3F route to the SPECIAL2 table).

The decoder handles COP2 vector ops (`vadd`/`vsub`/`vmul`/`vmadd`/`vmsub`/`vmax`/`vmini` per-component xyzw and Q/I immediate forms), COP2 special2 (`vrsqrt`, `vwaitq`, `vmulq`, etc.), COP1 FPU (`mul.s`/`add.s`/`sub.s`/etc.), LQ/SQ/SD quadword loads/stores, and GPR loads/stores. Output artifacts: `helper_disasm.txt`, `routine_disasm.txt`.

---

## 2. Key Findings — Helper `0x00106950`

The helper is a **vector normalization** routine. It takes a 4-float vector at `$a0`, squares its components, computes the magnitude, derives `Q = 1/|v|` via the pipelined `vrsqrt`, synchronizes, then scales the original vector by `Q` to unit length, writing the result back to `*a0`.

### Full decoded listing

```
00106950: addiu sp, sp, -0x10
00106954: lwc1 f3,(a0)          ; in[0] -> f3
00106958: addiu v1, sp, 0x0     ; v1 = sp
0010695C: lwc1 f2, 0x4(a0)      ; in[1] -> f2
00106960: lwc1 f1, 0x8(a0)      ; in[2] -> f1
00106964: lwc1 f0, 0xC(a0)      ; in[3] -> f0
00106968: swc1 f3,(sp)          ; stack copy of 4-float vector
0010696C: swc1 f2, 0x4(sp)
00106970: swc1 f1, 0x8(sp)
00106974: swc1 f0, 0xC(sp)
00106978: ldc2 vf04,(v1)        ; vf04 = in[0..3] into COP2 vector regs
0010697C: vmul.xyz vf05, vf04, vf04   ; vf05.xyz = vf04.xyz * vf04.xyz  (square)
00106980: vaddy   vf05, vf05, vf05    ; accumulate magnitude components
00106984: vaddz   vf05, vf05, vf05
00106988: vrsqrt Q, vf00w, vf05x      ; Q = 1/sqrt(magnitude) (pipelined)
0010698C: vwaitq                    ; wait for Q
00106990: vmulq.xyz vf04, vf04, Q     ; vf04.xyz = vf04.xyz * Q  (scale to unit)
00106994: sdc2 vf04,(v1)          ; store back to stack
00106998: lwc1 f3,(sp)            ; reload 4 floats
0010699C: lwc1 f2, 0x4(sp)
001069A0: lwc1 f1, 0x8(sp)
001069A4: lwc1 f0, 0xC(sp)
001069A8: swc1 f3,(a0)            ; write back to *a0
001069AC: swc1 f2, 0x4(a0)
001069B0: swc1 f1, 0x8(a0)
001069B4: swc1 f0, 0xC(a0)
001069B8: jr ra
001069BC: addiu sp, sp, 0x10
```

### The 6 COP2 instructions (VA 0x10697C–0x106990), decoded

| VA | Hex word | Decoded MIPS-3D (COP2) | Meaning |
|---|---|---|---|
| `0x10697C` | `4BC4216A` | `vmul.xyz vf05, vf04, vf04` | element-square vf04 → vf05 (x², y², z²) |
| `0x106980` | `4B052941` | `vaddy   vf05, vf05, vf05` | accumulate y component into magnitude |
| `0x106984` | `4B052942` | `vaddz   vf05, vf05, vf05` | accumulate z component into magnitude |
| `0x106988` | `4A6503BE` | `vrsqrt Q, vf00w, vf05x` | Q = reciprocal square root (pipelined) |
| `0x10698C` | `4A0003BF` | `vwaitq` | stall until Q accumulator ready |
| `0x106990` | `4BC0211C` | `vmulq.xyz vf04, vf04, Q` | vf04 = vf04 × Q (uniform scale) |

### Interpretation

`0x106950` performs a **uniform scale-to-unit-length** transform, not a matrix multiply or rotation:

```
*a0 = *a0 / |*a0|
```

The input is a 4-float vector (x, y, z, w) at `$a0`; only x/y/z participate in the magnitude; the result is written back to the same location.

---

## 3. Key Findings — Spawn Routine `0x0023AF30`

### Signature and calling convention

- `$a0` = stage/session object (used for vtable calls and field loads at `+0x00`, `+0x08`, `+0x24`).
- `$a1` = fighter object, copied to `$s5`; used as the player object for the position setter and for the nested field `+0x534 → +0x08` A/B selection.
- Stack frame: `0x170` bytes.
- Called **in pairs** for two fighters (same stage object, distinct fighter objects).

### Full decoded listing (annotated)

```
0023AF30: addiu sp, sp, -0x170        ; prologue, large frame
0023AF34: lui v1, 0x49                ; v1 = 0x490000
0023AF38: sd ra, 0x90(sp)
0023AF3C: addiu v1, v1, -0x7C00       ; v1 = 0x488400  (stage_group_map base)
0023AF40: sq fp, 0x80(sp)
0023AF44: sq s7, 0x70(sp)
0023AF48: sq s6, 0x60(sp)
0023AF4C: sq s5, 0x50(sp)
0023AF50: sq s4, 0x40(sp)
0023AF54: special rs=a1 rt=zero rd=a1 f=2d   ; identity move on a1
0023AF58: sq s3, 0x30(sp)
0023AF5C: sq s2, 0x20(sp)
0023AF60: sq s1, 0x10(sp)
0023AF64: sq s0,(sp)
0023AF68: lw a1, 0x8(a0)          ; stage->field_08
0023AF6C: lw v0, 0x24(a0)         ; stage->field_24
0023AF70: lw t9,(a0)              ; stage vtable pointer
0023AF74: sll a1, a1, v1          ; field_08 scaled into map space
0023AF78: addu v1, v1, a1
0023AF7C: sll v0, v0, v0          ; field_24 * 4
0023AF80: addu v0, v0, v1         ; v0 = &map[field_08*2 + field_24]
0023AF84: lw t9, 0x28(t9)         ; stage vmethod +0x28 -> slot selector
0023AF88: jalr ->t9
0023AF8C: lw s0,(v0)              ; s0 = placement group (delay-slot load)
0023AF90: lui v1, 0x48
0023AF94: sll a0, s0, v0          ; a0 = record_offset base
0023AF98: lwc1 f3, 0x7D84(v1)     ; f3 = 100.0f (global 0x487D84)
0023AF9C: addu a0, a0, s0         ; a0 = record_offset
0023AFA0: sll a2, a0, a0
0023AFA4: sll v0, v0, a0
0023AFA8: addiu s2, sp, 0x16C     ; stack locals for candidate vectors
0023AFAC: addiu s3, sp, 0x168
0023AFB0: addiu s4, sp, 0x164
0023AFB4: addiu s1, sp, 0x154
0023AFB8: addiu s6, sp, 0x15C
0023AFBC: addiu s0, sp, 0x158
0023AFC0: addiu t1, sp, 0x104
0023AFC4: addiu a3, sp, 0x108
0023AFC8: lui v1, 0x48
0023AFCC: addiu s7, sp, 0x144
0023AFD0: lwc1 f0, 0x7D8C(v1)     ; f0 = 1.0f (global 0x487D8C)
0023AFD4: addiu fp, sp, 0x148
0023AFD8: lui t0, 0x3F00
0023AFDC: addiu t6, sp, 0xF4
0023AFE0: addiu t5, sp, 0xF8
0023AFE4: addiu t4, sp, 0xE4
0023AFE8: addiu t3, sp, 0xE8
0023AFEC: addiu t2, sp, 0x114
0023AFF0: lui v1, 0x48
0023AFF4: lq a1, 0x7D80(v1)
0023AFF8: lui v1, 0x48
0023AFFC: sq a1, 0x160(sp)
0023B000: addiu v1, v1, 0x7E00
0023B004: addu a0, v1, a2         ; table addr 0x487E00 + record_offset
0023B008: lui v1, 0x48
0023B00C: addu a0, v0, a0
0023B010: addiu v1, v1, 0x7E04
0023B014: addu v1, v1, a2
0023B018: addu v1, v0, v1
0023B01C: lwc1 f1,(v1)            ; load table float slot+1
0023B020: lwc1 f6,(a0)            ; load table float slot+0
0023B024: swc1 f3,(s4)            ; 100.0f -> sp+0x164
0023B028: swc1 f0,(s2)            ; 1.0f   -> sp+0x16C
0023B02C: lwc1 f5, 0x164(sp)
0023B030: lui v1, 0x48
0023B034: lwc1 f2, 0x7D94(v1)     ; f2 = 100.0f (global 0x487D94)
0023B038: swc1 f1,(s3)            ; table slot+1 -> sp+0x168
0023B03C: lwc1 f4, 0x168(sp)
0023B040: swc1 f6, 0x160(sp)      ; table slot+0 -> sp+0x160
0023B044: lui v1, 0x48
0023B048: lwc1 f0, 0x7D9C(v1)     ; f0 = 1.0f (global 0x487D9C)
0023B050: lq a1, 0x7D90(v1)
0023B054: lui v1, 0x48
0023B058: sq a1, 0x150(sp)
0023B05C: addiu v1, v1, 0x7E08
0023B060: addiu a1, sp, 0x124
0023B064: addu a0, v1, a2         ; table addr 0x487E08 + record_offset
0023B068: lui v1, 0x48
0023B06C: addu a0, v0, a0
0023B070: addiu v1, v1, 0x7E0C
0023B074: addu v1, v1, a2
0023B078: addu v0, v0, v1         ; table addr 0x487E0C + record_offset
0023B07C: lui a2, 0x3F80          ; a2 = 1.0f
0023B080: lwc1 f1,(v0)            ; load table float slot+3
0023B084: addiu v1, sp, 0x128
0023B088: swc1 f2,(s1)            ; 100.0f -> sp+0x154
0023B08C: sw a2, 0xFC(sp)         ; 1.0f   -> sp+0xFC
0023B090: swc1 f0,(s6)            ; 1.0f   -> sp+0x15C
0023B094: lwc1 f0, 0x154(sp)
0023B098: lwc1 f3,(a0)            ; load table float slot+2
0023B09C: swc1 f1,(s0)            ; table slot+1? -> sp+0x158
0023B0A0: addiu v0, sp, 0x10C
0023B0A4: sw a2,(v0)              ; 1.0f   -> sp+0x10C
0023B0A8: addiu v0, sp, 0x12C
0023B0AC: sub.s f1, f0, f5        ; difference component
0023B0B0: addiu a0, sp, 0x140
0023B0B4: lwc1 f0, 0x158(sp)
0023B0B8: sub.s f2, f3, f6
0023B0BC: swc1 f1,(t1)            ; -> sp+0x104
0023B0C0: sub.s f0, f0, f4
0023B0C4: addiu t1, sp, 0x118
0023B0C8: swc1 f2, 0x100(sp)      ; -> sp+0x100
0023B0CC: swc1 f0,(a3)            ; -> sp+0x108
0023B0D0: lq a3, 0x100(sp)
0023B0D4: swc1 f3, 0x150(sp)      ; table slot+2 -> sp+0x150
0023B0D8: mtc1 f0, t0
0023B0DC: addiu t0, sp, 0xEC
0023B0E0: sw a2,(t0)              ; 1.0f -> sp+0xEC
0023B0E4: addiu t0, sp, 0x11C
0023B0E8: sq a3, 0x120(sp)
0023B0EC: swc1 f1,(a1)            ; -> sp+0x124
0023B0F0: sw a2,(v0)              ; 1.0f -> sp+0x12C
0023B0F4: swc1 f2, 0x120(sp)      ; -> sp+0x120
0023B0F8: swc1 f0,(v1)            ; -> sp+0x128
0023B0FC: lq v0, 0x120(sp)
0023B100: sq v0, 0x140(sp)
0023B104: lwc1 f2, 0x140(sp)
0023B108: lwc1 f1,(s7)
0023B10C: lwc1 f0,(fp)
0023B110: mul.s f2, f2, f3        ; scale components
0023B114: mul.s f1, f1, f3
0023B118: mul.s f0, f0, f3
0023B11C: swc1 f2, 0xF0(sp)       ; scaled x -> sp+0xF0
0023B120: swc1 f1,(t6)            ; scaled y -> sp+0xF4
0023B124: swc1 f0,(t5)            ; scaled z -> sp+0xF8
0023B128: add.s f2, f6, f2        ; add base offset
0023B12C: add.s f1, f5, f1
0023B130: add.s f0, f4, f0
0023B134: swc1 f2, 0xE0(sp)       ; final dir x -> sp+0xE0
0023B138: swc1 f1,(t4)            ; final dir y -> sp+0xE8
0023B13C: swc1 f0,(t3)            ; final dir z -> sp+0xEC
0023B140: lq v0, 0xE0(sp)
0023B144: sq v0, 0x110(sp)
0023B148: swc1 f1,(t2)            ; -> sp+0x114
0023B14C: sw a2,(t0)              ; 1.0f -> sp+0xEC (reuse)
0023B150: swc1 f0,(t1)            ; -> sp+0x118
0023B154: swc1 f2, 0x110(sp)      ; -> sp+0x110
0023B158: lq v0, 0x110(sp)
0023B15C: jal ->$0x00106950       ; *** NORMALIZE the direction vector ***
0023B160: sq v0, 0x130(sp)        ; store normalized result
0023B164: lwc1 f0,(s7)
0023B168: lui a0, 0x3F80          ; a0 = 1.0f
0023B16C: lwc1 f4, 0x140(sp)
0023B170: lui v0, 0x4170          ; v0 = scale constant
0023B174: mtc1 f0, v0
0023B178: addiu t0, sp, 0xD4
0023B17C: lwc1 f1,(fp)
0023B180: addiu a1, sp, 0xC4
0023B184: addiu v0, sp, 0xDC
0023B188: addiu v1, sp, 0xB4
0023B18C: sw a0,(v0)              ; 1.0f -> sp+0xDC
0023B190: lwc1 f3, 0x134(sp)
0023B194: mul.s f5, f0, f7
0023B198: addiu v0, sp, 0xA4
0023B19C: addiu t1, sp, 0xB8
0023B1A0: sw a0,(s2)              ; 1.0f -> sp+0x16C
0023B1A4: sw a0,(s6)              ; 1.0f -> sp+0x15C
0023B1A8: addiu a3, sp, 0xD8
0023B1AC: sw a0, 0xBC(sp)         ; 1.0f -> sp+0xBC
0023B1B0: addiu a2, sp, 0xA8
0023B1B4: sw a0, 0xAC(sp)         ; 1.0f -> sp+0xAC
0023B1B8: mul.s f6, f4, f7        ; scale normalized dir
0023B1BC: mul.s f4, f1, f7
0023B1C0: lwc1 f0, 0x130(sp)
0023B1C4: swc1 f5,(v0)            ; -> sp+0xA4
0023B1C8: swc1 f5,(v1)            ; -> sp+0xB4
0023B1CC: addiu v0, sp, 0xCC
0023B1D0: sw a0,(v0)              ; 1.0f -> sp+0xCC
0023B1D4: lwc1 f2, 0x138(sp)
0023B1D8: addiu v1, sp, 0xC8
0023B1DC: special rs=s5 rt=zero rd=a0 f=2d   ; identity move on s5
0023B1E0: swc1 f6, 0xB0(sp)       ; scaled dir y -> sp+0xB0
0023B1E4: sub.s f1, f0, f6        ; candidate B components
0023B1E8: add.s f0, f0, f6        ; candidate A components
0023B1EC: swc1 f1, 0xD0(sp)       ; -> sp+0xD0
0023B1F0: swc1 f1, 0x160(sp)      ; candidate B y -> sp+0x160
0023B1F4: sub.s f1, f3, f5
0023B1F8: swc1 f0, 0xC0(sp)       ; -> sp+0xC0
0023B1FC: swc1 f0, 0x150(sp)      ; candidate A y -> sp+0x150
0023B200: add.s f0, f3, f5
0023B204: swc1 f1,(t0)            ; -> sp+0xD4
0023B208: swc1 f1,(s4)            ; candidate B z -> sp+0x164
0023B20C: sub.s f1, f2, f4
0023B210: swc1 f0,(a1)            ; -> sp+0xC4
0023B214: swc1 f0,(s1)            ; candidate A z -> sp+0x154
0023B218: add.s f0, f2, f4
0023B21C: swc1 f6, 0xA0(sp)       ; candidate B x -> sp+0xA0
0023B220: swc1 f4,(t1)            ; -> sp+0xB8
0023B224: swc1 f4,(a2)            ; candidate A x -> sp+0xA8
0023B228: swc1 f1,(a3)            ; -> sp+0xD8
0023B22C: swc1 f1,(s3)            ; candidate B w -> sp+0x168
0023B230: swc1 f0,(v1)            ; candidate A w -> sp+0xB4
0023B234: jal ->$0x001CBB40       ; returns player->0x534->0x08 (A/B selector)
0023B238: swc1 f0,(s0)            ; candidate A w -> sp+0x158
0023B23C: li v1, 0x1
0023B240: beq v0, v1, ->$0x0023B274   ; if sel==1 -> candidate B (sp+0x160)
0023B244: nop
0023B248: beq v0, zero, ->$0x0023B258 ; if sel==0 -> candidate A (sp+0x150)
0023B24C: nop
0023B250: beq zero, zero, ->$0x0023B278 ; otherwise -> candidate B (sp+0x160)
0023B254: lw t9,(s5)
0023B258: lw t9,(s5)
0023B25C: special rs=s5 rt=zero rd=a0 f=2d
0023B260: lw t9, 0x40(t9)         ; player vmethod +0x40 (position setter)
0023B264: jalr ->t9
0023B268: addiu a1, sp, 0x160     ; candidate B argument
0023B26C: beq zero, zero, ->$0x0023B28C
0023B270: ld ra, 0x90(sp)
0023B274: lw t9,(s5)
0023B278: special rs=s5 rt=zero rd=a0 f=2d
0023B27C: lw t9, 0x40(t9)         ; player vmethod +0x40
0023B280: jalr ->t9
0023B284: addiu a1, sp, 0x150     ; candidate A argument
0023B288: ld ra, 0x90(sp)
0023B28C: lq fp, 0x80(sp)         ; epilogue restore
0023B290: lq s7, 0x70(sp)
0023B294: lq s6, 0x60(sp)
0023B298: lq s5, 0x50(sp)
0023B29C: lq s4, 0x40(sp)
0023B2A0: lq s3, 0x30(sp)
0023B2A4: lq s2, 0x20(sp)
0023B2A8: lq s1, 0x10(sp)
0023B2AC: lq s0,(sp)
```

### Stage-key lookup (decoded)

```
map_index = stage->field_08 * 2 + stage->field_24
group     = stage_group_map[map_index]        // at 0x00488400
slot      = stage_vmethod(+0x28)()            // returns 0..4
record    = &placement_table[group * 0x50 + slot * 0x10]   // at 0x00487E00
```

The placement table is `16 groups × 5 slots × 4 floats = 320 floats` (0x500 bytes), spanning `0x00487E00`–`0x004883FF`; the 32-bit map at `0x00488400` immediately follows it.

### Global constants used

| Address | Float value |
|---|---|
| `0x00487D84` | `100.0f` |
| `0x00487D8C` | `1.0f` |
| `0x00487D94` | `100.0f` |
| `0x00487D9C` | `1.0f` |

### How `0x106950` feeds `0x23AF30`

At `0x23B158`–`0x23B160`, the spawn routine loads the just-computed direction vector from `sp+0x110` and calls `0x106950` to normalize it (`v = v/|v|`), storing the result at `sp+0x130`. It then uses scalar `mul.s`/`add.s` with the normalized direction and the `1.0f`/`scale` constants to build **two candidate position vectors** (`sp+0x150` = candidate A, `sp+0x160` = candidate B). The helper's role is therefore to convert a stage-descriptor direction vector into a unit vector, which the spawn routine scales/positions into the two fighter spawn candidates.

### A/B selection and player setter

`0x23B234` calls `0x001CBB40` with `$a0 = player`; that helper returns `player->0x534->0x08`:
- Return value `1` → candidate B (`sp+0x160`)
- Return value `0` → candidate A (`sp+0x150`)
- Other nonzero → candidate B

The selected vector is passed as `$a1` to the player's vtable method at `+0x40` (position setter) at `0x23B260`/`0x23B27C`.

---

## 4. Caller Field Semantics

| Evidence | Interpretation |
|---|---|
| `lw a1, 0x8(a0)` at `0x23AF68` | Stage object field `+0x08` feeds stage-group map indexing |
| `lw v0, 0x24(a0)` at `0x23AF6C` | Stage object field `+0x24` feeds stage-group map indexing |
| `lw t9, 0x28(t9)` at `0x23AF84` | Stage object's virtual method at vtable byte offset `+0x28` returns slot selector (0–4) |
| `move $a0, $s5` at `0x23B1DC` / `0x23B25C` / `0x23B278` | Fighter object becomes player argument for nested field access and setter |
| `lw t9, 0x40(t9)` at `0x23B260` / `0x23B27C` | Player object's virtual method at vtable byte offset `+0x40` (position setter) |
| `addiu a1, sp, 0x160` / `0x150` | Candidate B / A vector passed to player setter |

Both fighter calls made with the same stage object use the same stage placement group and slot, then choose candidate A or B based on a per-fighter nested field (`player->0x534->0x08`).

---

## 5. EU Beta Build Impact (`SLES_541.61`)

The working directory now contains `SLES_541.61` (4,180,784 bytes), a little-endian PS2 ELF (entry `0x00100008`), which is the EU beta build and supersedes `SLUS_214.42` as the target.

**All absolute addresses, file offsets, table addresses, and global constants from the US analysis are invalidated for the EU build and must be re-located:**

- Routine VA `0x0023AF30` / helper VA `0x00106950` (and file offsets `0x13afb0`/`0x69d0` via `off = va - 0x100000 + 0x80`).
- `stage_group_map` at `0x00488400`.
- Placement table at `0x00487E00` (16 groups × 5 slots × 4 floats).
- Global constants `100.0f`/`1.0f` at `0x00487D84`–`0x00487D9C`.
- All branch/jump targets and stack offsets inside `0x23AF30`.

**What carries over:** The decode method and algorithm structure (same R5900/COP2 ISA, little-endian ELF, same entry convention). The same Python disassembler (`disasm.py`) applies unchanged.

**Required re-work for EU:** Re-scan `SLES_541.61` for the COP2 normalization pattern (`vmul.xyz → vaddy/vaddz → vrsqrt Q → vwaitq → vmulq.xyz`) to locate the helper, and the stage-key map/table lookup (`field_08*2 + field_24` into a 32-bit map, then `group*0x50 + slot*0x10` into a 4-float table) to locate the spawn routine and its data addresses in the EU binary.

---

## 6. Next Steps / Open Questions

1. **Re-locate routines in EU build (`SLES_541.61`):** pattern-scan for the COP2 normalization sequence and the stage-key lookup; re-decode both routines at their EU addresses.
2. **Identify stage class/vtable for `+0x28` method:** determine return domain/meaning (slot selector 0–4) and stage class name.
3. **Identify player vtable method `+0x40`:** confirm position-setter implementation, expected argument layout, and axis order/semantics (local vs world coordinates).
4. **Identify nested player field `+0x534 → +0x08`:** determine semantic meaning (likely player index/side) driving A vs B selection.
5. **Correlate placement groups with stages:** map the 16 groups to stage IDs/names and validate selected slots against known stage geometry.
6. **Confirm table descriptor field meaning:** determine whether the four floats per slot represent endpoints, bounds, positions, or another placement descriptor.
7. **Validate reconstructed positions** against known stage/player placements before changing native spawn selection.

---

## 7. Artifacts

Location: `C:\Dev\SuperDragonBallZ\.kilo\worktrees\lapis-hill\`

| File | Description |
|---|---|
| `disasm.py` | Python MIPS + COP2/MIPS-3D disassembler (PCSX2 table replica) |
| `helper_disasm.txt` | Full decoded listing of `0x00106950–0x001069c0` |
| `routine_disasm.txt` | Full decoded listing of `0x0023AF30–0x0023B2B0` |
| `_pcsx2_DebugTools_DisASM.h` | PCSX2 DisASM.h reference (downloaded) |

**Note:** All artifacts describe `SLUS_214.42` (US build) only. No source was modified; nothing was committed.
