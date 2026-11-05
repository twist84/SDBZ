# Recompiled Code — GAME.DAT LBA Map and Archive Layout

## 1. Disc Layout (from .ps2-disc/layout.tsv)

The extracted disc folder at `C:\Dev\ps2-web-recomp\sdbz\games\SuperDragonBallZ\disc\` contains a `.ps2-disc/layout.tsv` file that records original ISO 9660 LBAs for every file on the disc:

| LBA | Size (bytes) | File |
|-----|-------------|------|
| 0 | 36864 | .ps2-disc/metadata/00000000.bin |
| 257 | 12288 | .ps2-disc/metadata/00000101.bin |
| 286 | 54 | SYSTEM.CNF |
| 287 | 275345 | IOPRP300.IMG |
| 422 | 7417 | MCSERV.IRX |
| 426 | 44869 | PADMAN.IRX |
| 448 | 6641 | SIO2MAN.IRX |
| 452 | 71133 | ARKD_DVD.IRX |
| 487 | 76469 | CRI_ADXI.IRX |
| 525 | 28661 | LIBSD.IRX |
| 539 | 95909 | MCMAN.IRX |
| 586 | 4180784 | SLES_541.61 |
| 2628 | 14 | MOVIE/0FLIST.DIR |
| 2629 | 737280 | MOVIE/ATARI.SFD |
| 2989 | 41445376 | MOVIE/OP1.SFD |
| 23226 | 31930368 | MOVIE/OP_PAL.SFD |
| 38817 | 46405632 | MOVIE/OP_USA.SFD |
| **1712134** | **98304** | **INFO.DAT** |
| **1712182** | **730050560** | **GAME.DAT** |
| 2068656 | 131072 | .ps2-disc/metadata/001f90b0.bin |

**GAME.DAT disc location:**
- Start LBA: **1712182** (0x1A1C26)
- Size: **730,050,560 bytes** = **356,470 sectors** (2048 bytes/sector)
- End LBA (exclusive): **2,068,652** (0x1F2E7C)
- Next entry starts at LBA 2,068,656 (4-sector gap after GAME.DAT)

**GAME.DAT on disc spans LBA 1,712,182 through 2,068,651 inclusive.**

**SYSTEM.CNF** (LBA 286, 54 bytes):
```
BOOT2 = cdrom0:\SLES_541.61;1
VER = 0.90
VMODE = PAL
```

## 2. INFO.DAT Decoded Contents

INFO.DAT is 98,304 bytes at disc LBA 1,712,134. Decoded using the native codec (nibble-swap + bit inversion + key subtraction, key = first 16 bytes `7F 77 77 77 2E 61 72 69 6B 61 2E 63 6F 2E 6A 70` = `\x7fwww.arika.co.jp`):

- **File count**: 1,934 records
- **Record size**: 0x30 (48) bytes each
- **Record format**: filename[32] at +0x00, unknown u32 at +0x20, sector u32 at +0x24, sectors u32 at +0x28, byte_size u32 at +0x2C
- **Table range**: 0x30 to 0x16AD0 (header + 1934 × 48 bytes)
- **Padding**: ~5,424 encoded bytes (0x40 filled) after the table

### Indexed Region in GAME.DAT

| Metric | Value |
|--------|-------|
| Last indexed sector | 338,550 (entry 1933: `trc/06/fin06.pmt`) |
| Indexed byte offset | 693,350,400 (0x2953B000) |
| Total GAME.DAT size | 730,050,560 bytes |
| Unindexed tail | 36,700,160 bytes (~35 MiB, zero-filled) |

### File Types in GAME.DAT

| Extension | Count | Description |
|-----------|-------|-------------|
| pix | 729 | FPIX texture atlases (with TIM2 chunks) |
| mdl | 475 | FMDL model containers |
| ase | 290 | ASE sound-effect banks |
| ebz | 147 | EBZ effect data |
| spt | 77 | SPT script tokens |
| pmt | 57 | RFSM/L3 motion packages |
| ams | 54 | AMS music streams |
| cht | 43 | CHT hit/collision data |
| ani | 20 | ANI facial timelines |
| hit | 14 | HIT collision data |
| stg | 13 | STG stage data |
| pas | 7 | PAS files |
| ico | 2 | ICO icons |
| txt, fon, bak, 9, 10, 2 | 1 each | Miscellaneous |

### Top 10 Largest Files in GAME.DAT

| Size (bytes) | Sector in GAME.DAT | Name |
|-------------|-------------------|------|
| 8,783,872 | 261,428 | sqm/st03a_lp.ams |
| 8,180,600 | 8,222 | dis/d03.pix |
| 7,473,152 | 279,294 | sqm/st05b_lp.ams |
| 7,211,008 | 266,166 | sqm/st03b_lp.ams |
| 6,686,720 | 275,516 | sqm/st05a_lp.ams |
| 6,162,432 | 244,706 | sqm/ren_lp.ams |
| 5,769,216 | 282,943 | sqm/st06a.ams |
| 5,507,072 | 258,482 | sqm/st02a_lp.ams |
| 5,507,072 | 271,802 | sqm/st04b_lp.ams |
| 5,376,000 | 286,145 | sqm/st07a_lp.ams |

All assets are stored contiguously at `sector_offset * 2048` within GAME.DAT, with `sector_count = ceil(byte_size / 2048)`.

## 3. EE-Side GAME.DAT Access

### INFO.DAT Record Lookup — Found in Generated Code

**`unit_0030.cpp`** (VA `0x001C0000`–`0x001C4000`):
- **Entry 11u → `L_001C0050` at line 681**: Walks 48-byte (0x30) INFO.DAT records
- **ELF correspondence**: ELF address 0x1C0050 (confirmed by research: "EE lookup at 0x1C0050 walks 0x30-byte records")
- Reads `byte_size` at record offset +44 (0x2C) — lines 701, 718
- Record stride: **+48 bytes** (line 721: `c.r[18] += 48u`)
- Computes record offset: `index * 3 << 4 = index * 48` — lines 735-738
- Reads at `base + index*48 + 92` — line 739
- Calls `unit_0024` string comparison helpers (entries 91u at L_001915F8 line 2062, 83u at L_00191398 line 1893) for filename matching

### String Comparison Helpers — unit_0024.cpp

| Entry | Label | Line | VA | Function |
|-------|-------|------|-----|----------|
| 71u | `L_00191000` | 1637 | `0x00191000` | Byte-by-byte string compare with case handling |
| 83u | `L_00191398` | 1893 | `0x00191398` | SIMD string compare (16-byte chunks) |
| 84u | `L_00191410` | 1925 | `0x00191410` | SIMD string compare |
| 87u | `L_001914E0` | 1986 | `0x001914E0` | Byte-by-byte string copy |
| 91u | `L_001915F8` | 2062 | `0x001915F8` | SIMD string compare |

### Sector-to-Byte Conversion — Found Across 25+ Units

The `<< 11` shift (multiply by 2048 = sector size) appears in units 0006, 0007, 0008, 0009, 000A, 000C, 000F, 0011, 0012, 0014, 0016, 0018, 0019, 0020, 0021, 0030, 008B, 00D3, 00DA, 00DB, 00DD, 00E0, 00E1, 00E7, 00E8 — converting sector numbers to byte offsets for GAME.DAT reads.

Example: `unit_0030.cpp:2411`: `set32(c.r[5], c.r[2].w[0] << 11)` then calls `unit_002F(c, 392u)`.

## 4. GAME.DAT LBA References in Generated Units — NOT FOUND

| Search | Result |
|--------|--------|
| `0x1A1C26` (1712182 = GAME.DAT start LBA) | No matches |
| `1712182` (decimal) | No matches |
| `0x1F2E7C` (end LBA exclusive) | No matches |
| `2068652` (decimal end) | No matches |
| `\bsector\b`, `\bLBA\b`, `\bdisc\b`, `\bSceCd\b` (in units 00DE-00FF) | No matches |

**Conclusion**: The GAME.DAT disc LBA (1,712,182 = 0x1A1C26) does **not** appear as a constant anywhere in the 240 generated EE units. This is expected because:

1. The EE accesses GAME.DAT **through the IOP** via SIF commands, not directly
2. Sector offsets within GAME.DAT come from **INFO.DAT records** (read at runtime), not hardcoded constants
3. The absolute disc LBA = `1712182 + info_dat_sector_offset` is computed by the IOP (ARKD_DVD module at IOP 0x67800)
4. The EE only sees decoded data in RAM (EE RAM 0x015CE400 per research)

## 5. IOP-Side (Not in Generated EE Units)

From `research/archive_and_container_notes.md`:

| Component | IOP Address | Notes |
|-----------|-------------|-------|
| ARKD_DVD module | 0x67800 | INFO.DAT/GAME.DAT decoder module |
| Decoder routine | 0x67AC0 (module RVA 0x2C0) | Nibble-swap + inversion + key subtraction |
| Nibble swaps | 0x67B88/0x67B8C | Byte nibble swap |
| Bit inversion | 0x67BBC | `~x` operation |
| Key subtraction | 0x67BF4 | `x - key[i & 15]` |
| Table pointer | 0x72B04 | Points to decoded INFO.DAT table |
| Decoded buffer | EE RAM 0x015CE400 / IOP RAM 0x000BE300 | Confirmed in stage_stall.bin snapshot |

All IOP addresses are outside the EE VA range (0x00100000–0x004E3FFF) covered by the generated units.

## 6. Files in GAME.DAT by Directory (Top Directories)

Decoded INFO.DAT reveals these directory prefixes (from filenames):

| Directory | File Count | Example Files |
|-----------|-----------|---------------|
| dis/ | ~100 | main_menu.pix, result.pix, d00.pix, d01.pix, d03.pix |
| dis/d00/ | ~15 | d00t0101.pix, d00t0102.pix, ... |
| dis/d01/ | ~30 | d01t02.pix, d01t0301.pix, ... |
| sqm/ | ~50 | st03a_lp.ams, st05b_lp.ams, ren_lp.ams, ... |
| trc/ | ~100 | efin06.mdl, efin06.pix, fin06.ase, fin06.pmt |
| trc/06/ | ~20 | efin0208p0608.ebz, efin06.mdl, fin06.ase |

Note: Directory entries themselves are not separate records in INFO.DAT — directories are inferred from filename prefixes (any entry name that is a proper prefix of another entry is treated as a directory).

## 7. Cross-Reference: Native C++ vs Generated Code

| Aspect | Native C++ (`source/core/info_archive.cpp`) | Generated EE Units |
|--------|--------------------------------------------|---------------------|
| INFO.DAT decode | Implemented (nibble swap + invert + key sub) | Not present (IOP-side) |
| INFO.DAT record parsing | 48-byte records, 5 fields each | unit_0030.cpp L_001C0050 walks records |
| Record fields | +0x00 name, +0x20 unknown, +0x24 sector, +0x28 sectors, +0x2C byte_size | Reads +44 (byte_size), +48 (stride), +92 |
| GAME.DAT read | `sector * 2048`, read `byte_size` | `<< 11` shifts in 25+ units |
| Disc LBA for GAME.DAT | 1712182 (from layout.tsv) | Not referenced in EE units |
| Codec key | First 16 bytes of INFO.DAT | Not present (IOP-side) |

## 8. Summary

GAME.DAT occupies **disc LBA 1,712,182 through 2,068,651** (730,050,560 bytes, 356,470 sectors). It contains 1,934 assets indexed by INFO.DAT, including 729 FPIX textures, 475 FMDL models, 290 ASE sound banks, and 54 AMS music streams. The indexed data ends at sector 338,550 within GAME.DAT (byte 693,350,400), with a ~35 MiB zero-filled unindexed tail.

The GAME.DAT disc LBA does **not** appear as a constant in any generated EE unit. Disc access is mediated through the IOP (ARKD_DVD module) via SIF commands. The EE-side code only performs INFO.DAT record lookup (`unit_0030.cpp` L_001C0050) and sector-to-byte conversion (`<< 11`), with all actual sector reads handled by the host/runtime `ps2::disc::` abstraction layer.
