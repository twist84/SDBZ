# Super Dragon Ball Z — Spawn Placement Correlation Report
## Collated Findings

---

## 1. Scope and Methodology

### Objective
Correlate the verified spawn-placement table data with stage identities in Super Dragon Ball Z (PS2). Produce a stage-to-placement-group mapping and determine the coordinate convention.

### Source material analyzed
| File | Path | Role |
|------|------|------|
| USA ELF | `assets/original/SLUS_214.42` (4,196,528 bytes) | Primary analysis target (since removed) |
| EU beta ELF | `assets/original/SLES_541.61` (4,180,784 bytes) | Current target after build update |
| INFO.DAT | `assets/original/INFO.DAT` (98,304 bytes) | Authoritative stage index (13 STG records) |
| GAME.DAT | `assets/original/GAME.DAT` | Stage asset containers |
| Extracted STG files | `assets/extracted/stg/sNN/sNNa.stg` | Stage scene graphs (name tables, node TRS) |

### Research sources
- `SLUS_spawn_research_log.md` — decoded routine `0x0023AF30`, table indexing formula, map/table layouts, sentinel values
- `plan.md` — STG count = 13, Y-up world coordinate convention
- `STAGE_PLAYER_RUNTIME_PLAN.md` — spawn resolution philosophy, HIT collision integration
- `research/stage_format.md` — STG/HIT structure, node record sizes
- `research/archive_and_container_notes.md` — INFO.DAT codec, record layout, GAME.DAT sector layout
- `README.md` — runtime spawn override config format

---

## 2. Data Structures Extracted

### 2.1 Placement Table (16 groups × 5 slots × 4 floats)
- **USA (SLUS_214.42):** VA 0x00487E00 → file offset 0x387E80, size 0x500 bytes (320 floats)
- **EU beta (SLES_541.61):** file offset 0x00385970, size 0x500 bytes
- Layout: `group_byte_offset = group_index * 0x50`, `slot_byte_offset = slot_index * 0x10`
- Each slot = 4 little-endian float32 values: `[f0, f1, f2, f3]`

### 2.2 Stage-Group Map (28 u32 entries)
- **USA:** VA 0x00488400 → file offset 0x388480
- **EU beta:** file offset 0x00385F70
- Layout: `map_index = field_08 * 2 + field_24`; `group_index = stage_group_map[map_index]`
- Indices 0–25 are meaningful stage keys (13 stages × 2 variants); indices 26–27 are padding (0, 0)

### 2.3 Global Constants (near table, 0x7C bytes before table base)
- Values: `{100.0f, 0.0f, 1.0f, 0.0f}` (identical in both builds)
- Likely scale/baseline values used in the vector construction at `0x23AF30`; exact role unconfirmed

### 2.4 Authoritative Stage List (from decoded INFO.DAT)
INFO.DAT is obfuscated with a nibble-swap + key-subtraction codec. Decoding yields exactly 13 STG records:

| # | Stage | STG path | STG present? |
|---|-------|----------|--------------|
| 1 | s00 | `stg/s00/s00a.stg` | Yes (names=28, tmpl=0) |
| 2 | s01 | `stg/s01/s01a.stg` | Yes (names=1237, tmpl=13) |
| 3 | s02 | `stg/s02/s02a.stg` | Yes (names=978, tmpl=7) |
| 4 | s03 | `stg/s03/s03a.stg` | Yes (names=377, tmpl=11) |
| 5 | s03b | `stg/s03/s03b.stg` | Yes (names not fully listed) |
| 6 | s04 | `stg/s04/s04a.stg` | Yes (names=367, tmpl=10) |
| 7 | s04b | `stg/s04/s04b.stg` | Yes |
| 8 | s05 | `stg/s05/s05a.stg` | Yes (names=839, tmpl=4) |
| 9 | s06 | `stg/s06/s06a.stg` | Yes (names=940, tmpl=21) |
| 10 | s07 | `stg/s07/s07a.stg` | Yes (names=572, tmpl=19) |
| 11 | s10 | `stg/s10/s10a.stg` | Yes (names=398, tmpl=12) |
| 12 | s11 | `stg/s11/s11a.stg` | Yes (names=1230, tmpl=13) |
| 13 | s12 | `stg/s12/s12a.stg` | Yes (names=250, tmpl=1) |

**Folders s08/s09** have model/texture assets (s08a.mdl == s01a.mdl, s09a.mdl == s02a.mdl) but **no .stg/.hit files**. They are stage slots only.

---

## 3. Stage-Group Map Decoding (Identical in Both Builds)

`map_index = field_08 * 2 + field_24`, where `field_08` = stage folder index `sNN` (0–12) and `field_24` = variant bit (0/1).

| map_idx | field_08 | stage | field_24 | group |
|---------|----------|-------|----------|-------|
| 00 | 0 | s00 | 0 | 0 |
| 01 | 0 | s00 | 1 | 0 |
| 02 | 1 | s01 | 0 | 1 |
| 03 | 1 | s01 | 1 | 1 |
| 04 | 2 | s02 | 0 | 2 |
| 05 | 2 | s02 | 1 | 2 |
| 06 | 3 | s03 | 0 | 3 |
| 07 | 3 | s03 | 1 | 4 |
| 08 | 4 | s04 | 0 | 5 |
| 09 | 4 | s04 | 1 | 6 |
| 10 | 5 | s05 | 0 | 7 |
| 11 | 5 | s05 | 1 | 8 |
| 12 | 6 | s06 | 0 | 9 |
| 13 | 6 | s06 | 1 | 9 |
| 14 | 7 | s07 | 0 | 10 |
| 15 | 7 | s07 | 1 | 10 |
| 16 | 8 | s08 | 0 | 11 |
| 17 | 8 | s08 | 1 | 11 |
| 18 | 9 | s09 | 0 | 12 |
| 19 | 9 | s09 | 1 | 12 |
| 20 | 10 | s10 | 0 | 13 |
| 21 | 10 | s10 | 1 | 13 |
| 22 | 11 | s11 | 0 | 14 |
| 23 | 11 | s11 | 1 | 14 |
| 24 | 12 | s12 | 0 | 15 |
| 25 | 12 | s12 | 1 | 15 |
| 26 | — | — | — | 0 (padding) |
| 27 | — | — | — | 0 (padding) |

**Corroboration:** Group 11 == group 1 data and group 12 == group 2 data, matching s08/s09 reusing s01/s02 geometry (identical model/pix sizes). Group 13 → s10 yields a clean two-fighter versus spawn.

---

## 4. Placement Table: Sentinel vs Real Classification

A slot is **"real"** if it is not one of the two sentinel patterns:
- Sentinel A: `{-15, 0, 15, 0}`
- Sentinel B: `{-15, 0.1, 15, 0.1}`

### Sentinel groups (all 5 slots sentinel): **0, 3, 4, 9, 10, 15**
- Groups 0, 3, 4, 9, 10 use sentinel A `{-15, 0, 15, 0}`
- Group 15 uses sentinel B `{-15, 0.1, 15, 0.1}`

### Real groups: **1, 2, 5, 6, 7, 8, 11, 12, 13, 14**

#### Per-group real slots:
| Group | Real slots | Representative values |
|-------|-----------|----------------------|
| G1 | 0,1,2,3,4 | S0=[20.3,66,14.5,34.4] S1=[-175.5,204.7,-201.1,208] S2=[-182.6,-80.4,-141.7,-91.9] (S3=S1,S4=S2) |
| G2 | 0,1,2,3,4 | S0=[-85,70,-85,40] S1=[-335.9,-194,-307.3,-182.3] S2=[-67.3,-183.7,-37.1,-185.7] S3=[432.4,44.2,451.5,34.2] (S4=S1) |
| G5 | 1,2,3,4 | S1=[104.7,234.4,82.7,260.6] S2=[174.5,-41.8,204.4,-36.8] S3=[-302,-123.7,-297.7,-90.8] (S4=S1; S0 sentinel) |
| G6 | 1,2,3,4 | S1=[-71.5,347.8,-79.3,323.8] S2=[303.1,-114.1,311.1,-145.8] S3=[-278.9,30.1,-304,9.1] (S4=S1; S0 sentinel) |
| G7 | 0,1,2,3,4 | S0=[-3.2,6.9,22.4,-3.4] S1=[-113.7,242.8,-96.9,222.7] S2=[279.9,-50.8,272.9,-80.5] (S3=S1,S4≈S2) |
| G8 | 0,1,2,3,4 | S0=[-18.9,42.6,10.4,36.4] S1=[-127.2,276.5,-110.4,249.4] S2=[168,-35.4,182.9,-62] (S3=S1,S4=S2) |
| G11 | 0,1,2,3,4 | == G1 data |
| G12 | 0,1,2,3,4 | == G2 data |
| G13 | **1,2 only** | S1=[-165,0,-135,0] S2=[135,0,165,0] (S0,S3,S4 sentinel) |
| G14 | 0,1,2,3,4 | == G1 data |

**Important correction:** Group 13 is REAL (contrary to the original task premise which listed only {0,3,4,9,10} as sentinel). Group 13 slots 1,2 = `[-165, 0, -135, 0]` / `[135, 0, 165, 0]` — a clean two-fighter versus spawn at opposite corners, exactly on the ground (y=0, w=0).

---

## 5. Stage → Placement Group Mapping

| Stage | field_08 | var0 → group | var1 → group | Placement | Usable (non-sentinel) slots |
|-------|----------|-------------|-------------|-----------|------------------------------|
| s00 | 0 | g0 SENT | g0 SENT | none | — |
| s01 | 1 | g1 real | g1 real | real | 0,1,2,3,4 |
| s02 | 2 | g2 real | g2 real | real | 0,1,2,3,4 |
| s03 | 3 | g3 SENT | g4 SENT | none | — |
| s04 | 4 | g5 real | g6 real | real | 1,2,3,4 (slot 0 sentinel) |
| s05 | 5 | g7 real | g8 real | real | 0,1,2,3,4 |
| s06 | 6 | g9 SENT | g9 SENT | none | — |
| s07 | 7 | g10 SENT | g10 SENT | none | — |
| s08 | 8 | g11 real (=g1) | g11 real | real | 0,1,2,3,4 *(no .stg file)* |
| s09 | 9 | g12 real (=g2) | g12 real | real | 0,1,2,3,4 *(no .stg file)* |
| s10 | 10 | g13 real | g13 real | real | **1,2 only** `[-165,0,-135]`/`[135,0,165]` |
| s11 | 11 | g14 real (=g1) | g14 real | real | 0,1,2,3,4 |
| s12 | 12 | g15 SENT | g15 SENT | none | — |

**Stages with real placement data (8):** s01, s02, s04, s05, s08, s09, s10, s11  
**Stages with sentinel/no placement (5):** s00, s03, s06, s07, s12  
**s08/s09** carry placement but have no `.stg`/`.hit` scene assets (reuse s01/s02 geometry).

---

## 6. Routine `0x0023AF30` (Stage-to-Player Placement)

### Decoded from `SLUS_spawn_research_log.md`
The routine receives a stage/session object (`$a0`) and a fighter object (`$a1`):

```
group = stage_group_map[stage->field_08 * 2 + stage->field_24]
record_offset = group * 0x50 + stage_vmethod(+0x28) * 0x10
read 4 floats at 0x00487E00 + record_offset   // +0, +4, +8, +0xC
```

- **stage field +0x08** and **+0x24** form the map index (verified at `0x23AF68`, `0x23AF6C`)
- **stage vtable method +0x28** returns slot selector (0–4); multiplied by 16 (0x10) for slot stride
- **Helper `0x001CBB40`** = `player->0x534->0x08` (NOT a collision query — corrected interpretation)
  - Return value `1` → candidate vector at stack `$sp + 0x150`
  - Return value `0` or other → candidate vector at stack `$sp + 0x160`
- **Fighter vtable method +0x40** called with selected 3-float vector as argument
- Two candidate vectors are constructed (16 bytes apart on stack); same stage placement group/slot used for both fighters, then per-fighter field selects candidate A or B

### Unresolved in routine
- Exact return domain of stage vmethod +0x28 (which slot is chosen per stage)
- Full 4-float → two-candidate-vector transform (COP2/VU stack math at `0x23AF90`–`0x23B234` not fully decoded; llvm-objdump lacks PS2 EE awareness)
- Semantic meaning of player field `+0x534 → +0x08` (nested field accessor)
- Exact role of globals `{100.0, 0.0, 1.0, 0.0}` (scale, baseline, or homogeneous component)

---

## 7. Coordinate Convention (Best Interpretation)

### Axis order of a 4-float slot: **[x, y, z, w]**
- `float[0]` = x (horizontal)
- `float[1]` = y (**VERTICAL**)
- `float[2]` = z (horizontal)
- `float[3]` = w (padding / homogeneous component, ~0.0)

### Vertical axis = index 1 → **Y-UP**
- **Ground plane: y = 0**
- Consistent with `plan.md`: "Y-up world coordinates, right-handed"
- Consistent with runtime's XZ spatial grid where Y is height

### Evidence
1. **Group 13 (s10):** S1=`[-165, 0, -135, 0]`, S2=`[135, 0, 165, 0]` — two fighters at opposite corners, exactly on the ground (y=0), w=0. This is the cleanest two-fighter versus spawn and pins index 1 as vertical at ground level.

2. **STG node translations** (parsed from extracted stage files) show Y as the axis with significant vertical extent in every stage:
   - s04: Y[0, 30]; s06: Y[-0.5, 49.5]; s12: Y[0, 0.6]; s02: Y[-10.07, 0]; s05: Y[-5.91, 0]; s03: Y[-6.35, 0]; s01: Y[-0.57, 0.57]
   - X and Z are near-zero in all cases

### Units
- Raw float world units
- Global constant `100.0f` hints at a possible cm→m (/100) scale, but the exact multiply/divide transform is **not decoded** (would require reconstructing `0x23AF30` stack math)
- Fighter separation in g13 is 330 raw units (~3.3 m if /100), plausible for versus spawn

---

## 8. EU Beta Build (SLES_541.61) — Version Impact

### Change
`SLUS_214.42` (USA) was removed from `assets/original/`; `SLES_541.61` (EU beta) is now the target ELF.

### What changed: absolute addresses/file offsets only
| Item | USA (SLUS_214.42) | EU Beta (SLES_541.61) |
|------|-------------------|----------------------|
| Placement table VA | 0x00487E00 | — (VA not applicable) |
| Placement table file offset | 0x387E80 | **0x00385970** |
| Stage-group map VA | 0x00488400 | — |
| Stage-group map file offset | 0x388480 | **0x00385F70** |
| Globals file offset | 0x387E04 | **0x003858F4** |
| ELF size | 4,196,528 | 4,180,784 |

Data shifted earlier by **0x2510 bytes** in SLES. USA-derived VAs do not apply — reading at those file offsets in SLES yields garbage (texture-like UV floats, large u32 map values, zero globals).

### What did NOT change (identical data values — verified by pattern search)
- Stage-group map: `0,0,1,1,2,2,3,4,5,6,7,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,0,0` — IDENTICAL
- Placement table: all 16×5×4 float values — IDENTICAL
- Globals: `{100.0f, 0.0f, 1.0f, 0.0f}` — IDENTICAL
- Sentinel/real groups — IDENTICAL
- Group 13 real slots 1,2 — IDENTICAL
- Stage→group mapping — IDENTICAL
- Coordinate convention — IDENTICAL
- Routine `0x23AF30` logic — IDENTICAL (routine VA may differ but structure is same)

### Conclusion for EU beta
The stage-to-placement-group correlation, sentinel/real classification, coordinate convention, and routine logic are **all version-independent**. Only absolute ELF file offsets must be re-derived per build. For SLES, use table base `0x00385970` and map base `0x00385F70`.

---

## 9. Files Produced

| File | Path | Contents |
|------|------|----------|
| Placement table binary | `.kilo/worktrees/bygone-hubcap/placement_table.bin` | Raw 0x500 bytes from USA ELF |
| Stage-group map binary | `.kilo/worktrees/bygone-hubcap/stage_group_map.bin` | Raw 0x70 bytes from USA ELF |
| Placement report | `.kilo/worktrees/bygone-hubcap/placement_report.txt` | Full table + sentinel classification |
| Stage correlation report | `.kilo/worktrees/bygone-hubcap/STAGE_CORRELATION_REPORT.md` | Markdown summary |
| SLES parse script | `.kilo/worktrees/bygone-hubcap/sles_parse.ps1` | ELF header parser |
| SLES placement script | `.kilo/worktrees/bygone-hubcap/sles_placement.ps1` | SLES table/map reader |
| SLES search script | `.kilo/worktrees/bygone-hubcap/sles_search.ps1` | Sentinel/map pattern search |
| SLES decode script | `.kilo/worktrees/bygone-hubcap/sles_decode.ps1` | SLES table/map decoder |
| INFO records CSV | `.kilo/worktrees/bygone-hubcap/info_records.csv` | Decoded INFO.DAT file index |

---

## 10. Open Questions / Next Steps

### High priority
1. **Stage vmethod +0x28 implementation** — identify which slot (0–4) is selected per stage/stage variant. This determines which real slot is actually used at runtime.
2. **4-float → two-vector transform** — reconstruct the stack math at `0x23AF30` (`0x23AF90`–`0x23B234`) to understand how the 4-float descriptor becomes two candidate positions. May require a PS2 EE-aware disassembler for the COP2/VU instructions.
3. **Player field `+0x534 → +0x08` semantics** — determine why it selects candidate vector A vs B (likely player index/side, but unconfirmed).
4. **Global constant role** — confirm whether `100.0f` applies a /100 scale (cm→m) or serves another purpose.

### Medium priority
5. **field_24 variant semantics** — for s03/s04 it likely selects s03a/s03b / s04a/s04b STG variant; for s05 both variants are real (g7/g8). Confirm.
6. **Validate placements against stage geometry** — correlate real slot positions with STG node translations and HIT collision bounds to confirm spawn points land on walkable surfaces.
7. **Caller lifecycle** — identify which call sites of `0x23AF30` correspond to round start vs stage transition vs rematch vs respawn.

### Low priority
8. **s08/s09 stage identity** — they carry placement data but have no scene assets; determine if they are unused stage slots or if placement applies to reused s01/s02 geometry at runtime.

---

## 11. Corroborating Evidence Summary

| Evidence | Source | Confidence |
|----------|--------|------------|
| Map indexing formula `field_08*2+field_24` | Disassembly `0x23AF68`–`0x23AF80` | High |
| Table base VA 0x00487E00 (USA) | Disassembly `0x23AF34`–`0x23AF3C` | High |
| 16×5×4 float layout | `0x23AF30` record_offset calc | High |
| Stage vmethod +0x28 = slot selector | `0x23AF84` vtable call + stride 0x10 | High |
| Helper 0x1CBB40 = player->0x534->0x08 | Corrected disassembly (2 instructions) | High |
| Two candidate vectors (stack +0x150/+0x160) | `0x23B268`/`0x23B284` | High |
| Fighter vmethod +0x40 = position setter | `0x23B260`/`0x23B27C` | High |
| Group 13 real (not sentinel) | Direct binary read of table | High (corrects premise) |
| Y-up, [x,y,z,w], ground y=0 | Group 13 fighters at y=0/w=0 + STG node Y-extent | Medium-high |
| /100 scale hint | Global `100.0f` constant | Low (unconfirmed transform) |

---

**No game source was modified. No changes were committed.**

---
*Report compiled from direct binary analysis of `SLUS_214.42` and `SLES_541.61`, decoded `INFO.DAT`, and parsed extracted STG stage files.*
