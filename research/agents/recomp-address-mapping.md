# ELF <-> ps2-web-recomp C++ Unit Address Mapping

## Source units analyzed
- `C:/Dev/ps2-web-recomp/sdbz/generated/SuperDragonBallZ/unit_004C.cpp` (VA 0x00230000-0x00233FFF)
- `C:/Dev/ps2-web-recomp/sdbz/generated/SuperDragonBallZ/unit_004E.cpp` (VA 0x00238000-0x0023BFFF)
- `C:/Dev/ps2-web-recomp/sdbz/generated/SuperDragonBallZ/unit_0000.cpp` (VA 0x00100000-0x00103FFF)
- `C:/Dev/ps2-web-recomp/sdbz/generated/SuperDragonBallZ/unit_0001.cpp` (VA 0x00104000-0x00107FFF)
- `C:/Dev/ps2-web-recomp/sdbz/generated/SuperDragonBallZ/unit_00E1.cpp` (VA 0x00484000-0x00487FFF)

## Unit-to-VA mapping rule
Each generated unit covers a 16 KiB window `0x00100000 + N*0x4000` (unit number N in hex). Dispatch guard: `pc - base < 16384`, then `entries_N[(pc-base)>>2]` indexes a sparse jump table. Labels `L_<VA>` are basic-block entry points. A VA is "in unit" only if inside the 16 KiB window AND present as a reachable label or declared data.

## Corrected address mappings

### 1. SLES spawn routine 0x0023ACC8 -> unit_004E.cpp (NOT unit_004C.cpp)

**Premise claimed unit_004C.cpp (0x00230000-0x00233FFF). That is wrong.**

- `0x0023ACC8 - 0x00230000 = 0xACC8 = 44232`, which is NOT `< 16384`. Outside unit_004C's window.
- Correct unit by VA arithmetic: `(0x23ACC8 - 0x100000) / 0x4000 = 0x4E` → **unit_004E.cpp**, VA `0x00238000-0x0023BFFF`. `0x0023ACC8` is inside this window (offset `0x2CC8`).
- unit_004E.cpp dispatch guard confirmed: `t - 0x00238000u < 16384` with `entries_004E[4096]`.
- **Code is present**: unit_004E.cpp contains labels spanning `L_0023AC14`, `L_0023AC28`, `L_0023AC30`, `L_0023AC40`, `L_0023AC50`, `L_0023AC70`, `L_0023AC90`, `L_0023ACF0` — the `0x23ACxx` block containing `0x23ACC8`.
- **Discrepancy**: `L_0023ACC8` is NOT itself a `L_*` label boundary — `grep` found no `ACC8` label. `0x23ACC8` lies mid-block between `L_0023AC90` and `L_0023ACF0`, reached by fall-through rather than as a direct dispatch entry. So `entries_004E[2866]` (= `(0x2CC8)>>2`) is `0` (not a dispatch target); the recomp's block boundary starts earlier (~0x23AC90) and falls through to 0x23ACC8.
- **Role**: This is CODE (the stage-to-player spawn routine), confirmed by unit_004E.cpp containing call instructions to other units (unit_006C, unit_006D) and the spawn routine's call/return structure.

### 2. SLES normalization helper 0x00106830 -> unit_0001.cpp (NOT unit_0000.cpp)

**Premise claimed unit_0000.cpp (0x00100000-0x00103FFF). That is wrong.**

- `0x00106830 - 0x00100000 = 0x6830 = 26672`, NOT `< 16384`. Outside unit_0000's window.
- Correct unit: `(0x106830 - 0x100000) / 0x4000 = 1` → **unit_0001.cpp**, VA `0x00104000-0x00107FFF`. `0x00106830` is inside (offset `0x2830`).
- **Confirmed present**: `unit_0001.cpp:245` `case 105u: goto L_00106830;` and `unit_0001.cpp:3131` `L_00106830:`. Exact label exists as a dispatch entry (entries_0000... entries_0001 index for offset 0x2830 >> 2 = 0xA18 = 2584 → entry 105).
- **Role**: CODE — vector normalization helper (`*a0 = *a0 / |*a0|` via VFPU: vmul.xyz → vaddy/vaddz → vrsqrt Q → vwaitq → vmulq.xyz). Called by the spawn routine at `0x23B15C` (US: 0x106950 / SLES: 0x106830).

### 3. SLES stage_group_map 0x00485EF0 -> unit_00E1.cpp VA window, but NO data declared

**In-range as address, but not a C++ data structure.**

- `0x00485EF0` is inside unit_00E1's window `0x00484000-0x00487FFF` (offset `0x1EF0`, index `entries_00E1[1980]`).
- unit_00E1.cpp is pure division-by-constant helper code (magic-number multiply blocks with `trap` on div-by-zero). It declares **no arrays** and performs **no `ld32`/`st32`/`ld64` memory reads at all** (grep found zero memory-access instructions; only `trap(c, 0x00485F90u...)` embedded return addresses).
- `entries_00E1[1980] = 0` — not a code block. **stage_group_map is not declared as data in unit_00E1.cpp.**
- **How it's actually accessed**: via absolute-address reads through the PS2 memory model from OTHER code units. E.g. unit_004C.cpp does `c.r[2] = 0x00480000; c.f[3] = ld32(c.r[2] + 17536)` → reads ELF data at absolute `0x004844C0`. The data lives in the ELF binary and is read at absolute VA, not materialized as a C++ array.

### 4. SLES placement table 0x004858F0 -> unit_00E1.cpp VA window, but NO data declared

- Inside unit_00E1's window (offset `0x18F0`, index `entries_00E1[1596] = 0`). Same situation: unit_00E1.cpp declares no placement table and performs no memory reads of it. **placement_table is not a C++ array in unit_00E1.cpp.** Pulled from ELF via absolute reads from whichever code unit needs it.

### 5. SLES global constants 0x00485874-0x0048588C -> unit_00E1.cpp VA window, but NO data declared

- Inside unit_00E1's window (offsets `0x1874`-`0x188C`, indices 1565-1569, all `0`). Same situation: no constant table declared. **Not a C++ data structure.** Pulled from ELF via absolute reads.

## ELF research findings (documented) and how they map

### Data structures (pulled from ELF at absolute VA, not declared as C++ arrays)

| Item | SLUS (US) VA | SLES (EU beta) VA | File offset (SLES) | Size | Notes |
|------|-------------|-------------------|--------------------|------|-------|
| stage_group_map | 0x00488400 | 0x00485EF0 | 0x385F70 | 28 u32 (0x70 bytes) | 28 entries, values 0-15 |
| placement_table | 0x00487E00 | 0x004858F0 | 0x385970 | 16 groups × 5 slots × 4 floats = 320 floats (0x500 bytes) | spans 0x4858F0-0x485DEF |
| global constants | 0x00487D84-0x487D9C | 0x00485874-0x48588C | — | 4 floats | {100.0f, ..., 1.0f, ...} |

**Data shift:** SLES data sections are uniformly **-0x2510** bytes vs SLUS. All data values are identical between builds; only absolute addresses/offsets differ.

### Indexing formulas (from disassembly, confirmed matching decompiled logic)

```
map_index    = stage->field_08 * 2 + stage->field_24       // stage+0x08, stage+0x24
group_index  = stage_group_map[map_index]                  // 28-entry map
slot_selector = stage->vtable[+0x28](stage)                // returns 0-4
record_offset = group_index * 0x50 + slot_selector * 0x10  // 0x50=80B=20 floats per group; 0x10=16B=4 floats per slot
record        = placement_table + record_offset            // 4 floats [x, y, z, w]
```

### Field offsets (confirmed by MIPS disassembly)

| Offset | Object | Semantics | Confirmed at |
|--------|--------|-----------|--------------|
| `+0x08` | Stage | Stage ID (0-12), feeds map index | MIPS `lw a1, 0x8(a0)` @ 0x23AF68 |
| `+0x24` | Stage | Variant/sub-stage bit (0/1), feeds map index | MIPS `lw v0, 0x24(a0)` @ 0x23AF6C |
| Vtable `+0x28` | Stage | Spawn slot selector (0-4) | `lw t9, 0x28(t9)` @ 0x23AF84 |
| `+0x58` | Owner/session | Stage object pointer | Caller analysis `owner+0x58` |
| `+0x60`/`+0x64` | Owner/session | Fighter #1 / #2 object pointers | Caller analysis |
| `+0x534` | Fighter | Side-selector sub-object pointer | `lw $v0, 0x534($a0)` @ helper 0x1CB9D0 |
| `+0x534+0x08` | Side sub-object | Binary side selector (0=P1/vecA, 1=P2/vecB) | `lw $v0, 0x08($v0)` delay slot |
| Vtable `+0x40` | Fighter | World-space position setter `set_pos(fighter*, vec3*)` | `lw t9, 0x40(t9)` @ 0x23B260/0x23B27C |

### Code routines (SLES → SLUS delta)

| Routine | SLES VA | SLUS VA | Delta | Generated unit |
|---------|---------|---------|-------|----------------|
| Spawn routine | 0x0023ACC8 | 0x0023AF30 | -0x268 | unit_004E.cpp (code, mid-block) |
| Normalization helper | 0x00106830 | 0x00106950 | -0x120 | unit_0001.cpp (label L_00106830) |
| Side-selector helper | 0x001CB9D0 | 0x001CBB40 | -0x170 | — (not among mapped units) |

## Discrepancy summary (corrected)

| Research address | Premise claimed unit | Actual result | Reason |
|------------------|---------------------|---------------|--------|
| 0x0023ACC8 (spawn routine) | unit_004C (0x230000-0x233FFF) | **unit_004E.cpp** | 0xACC8 > 0x3FFF; outside unit_004C window. Code present in unit_004E (0x238000-0x23BFFF), but `L_0023ACC8` is mid-block, not a label boundary. |
| 0x00106830 (normalization helper) | unit_0000 (0x100000-0x103FFF) | **unit_0001.cpp** | 0x6830 > 0x3FFF; outside unit_0000 window. Label `L_00106830` confirmed in unit_0001 (0x104000-0x107FFF). |
| 0x00485EF0 (stage_group_map) | unit_00E1 | **No data declared** | In-range but `entries_00E1[1980]=0`; unit_00E1 declares no arrays and does no memory reads. Pulled from ELF via absolute reads. |
| 0x004858F0 (placement table) | unit_00E1 | **No data declared** | In-range but `entries_00E1[1596]=0`; same as above. |
| 0x00485874-0x0048588C (globals) | unit_00E1 | **No data declared** | In-range but indices 1565-1569 all zero; same as above. |

## Conclusion / recommended next step

None of the five premise mappings hold exactly as stated:
- Two code routines are in **different units** than claimed (spawn → unit_004E.cpp; normalization helper → unit_0001.cpp).
- The three data items are **not declared as C++ arrays anywhere** in the generated units — they remain in the ELF binary and are pulled in at runtime via **absolute-address reads** through the PS2 memory model (`ld32`/`st32` on `0x00480000 + offset`, as seen in unit_004C.cpp). There is no `stage_group_map`/`placement_table`/globals C++ variable to map in unit_00E1.cpp.

To wire these into the native rebuild, reference the ELF absolute addresses directly (or the extracted data binaries at `.kilo/worktrees/bygone-hubcap/placement_table.bin` and `stage_group_map.bin`) rather than looking for declared arrays in unit_00E1.cpp. The code units that consume them (e.g. unit_004E.cpp for the spawn routine, unit_0001.cpp for the normalizer) are the correct implementation locations.
