# Recompiled Code — ELF Archive Loading (INFO.DAT / GAME.DAT) Search Results

## 1. Summary

The ELF research documents an **EE-side INFO.DAT lookup at ELF address 0x1C0050** that walks 0x30-byte (48-byte) records, plus an **IOP-side INFO.DAT decoder (ARKD_DVD module)** that performs the nibble-swap + key-subtraction codec. Verification against the generated EE units confirms:

- **EE-side lookup code EXISTS** at `unit_0030.cpp` entry 11u (`L_001C0050`, line 681), matching ELF address 0x1C0050
- **Nibble-swap codec does NOT exist in EE units** — it runs on the IOP (ARKD_DVD module at IOP 0x67800, routine at 0x67AC0)
- **GAME.DAT sector reading exists** via `<< 11` (multiply by 2048 = sector size) patterns across many units
- **String comparison helpers** in `unit_0024.cpp` are used by the INFO.DAT lookup to match filenames

All analysis is read-only against `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`. No files modified.

## 2. ELF Research Reference Points

| Research Detail | ELF Address | Notes |
|-----------------|-------------|-------|
| INFO.DAT decoder module | IOP 0x67800 (ARKD_DVD) | IOP-side, not in EE units |
| Decoder routine | IOP 0x67AC0 (module RVA 0x2C0) | IOP-side, not in EE units |
| Nibble swaps | IOP 0x67B88/0x67B8C | IOP-side |
| Bit inversion | IOP 0x67BBC | IOP-side |
| Key subtraction | IOP 0x67BF4 | IOP-side |
| Table pointer | IOP 0x72B04 | IOP-side |
| **EE lookup walks 0x30-byte records** | **ELF 0x1C0050** | **Found at unit_0030.cpp:681** |

## 3. EE-Side INFO.DAT Lookup — unit_0030.cpp L_001C0050

### Function Entry

**File:** `unit_0030.cpp` (VA `0x001C0000`–`0x001C4000`)
**Entry:** Entry 11u → `L_001C0050` at line 681
**ELF correspondence:** ELF 0x1C0050 — "EE lookup walks 0x30-byte records"

### Function Body Analysis (lines 681–900)

```
681: L_001C0050:
682:     set32(c.r[29], c.r[29].w[0] - 96u);        -- stack frame setup
...
686:     c.r[20].d[0] = c.r[4].d[0] + 0ull;        -- c.r[20] = search key pointer (c.r[4] = arg0)
687:     st128((c.r[29].w[0] + 32u) & ~15u, c.r[18]);
688:     st128((c.r[29].w[0] + 16u) & ~15u, c.r[17]);
690:     call unit_0024(c, 91u, depth+1);          -- returns INFO.DAT base pointer in c.r[2]
...
692:     c.r[3] = 0xFFFFFFFC (-4);                  -- alignment mask
693:     c.r[2] = c.r[2] & c.r[3];                  -- align base pointer
694:     c.r[1] = c.r[2] < 8 ? 1 : 0;               -- check size >= 8
696:     if (c.r[1] == 0) { c.r[17] = c.r[2] - 8; goto L_001C008C; }
697:     c.r[17] = 0;                               -- record index offset
...
700: L_001C008C:
700:     c.r[3] = ld32(c.r[28] - 10356);            -- INFO.DAT base pointer from stack
701:     c.r[2] = ld32(c.r[3] + 44u);               -- READ byte_size at record offset +44
702:     if (c.r[2] == 0) goto L_001C00D8;          -- no records / empty table
705:     c.r[18] = 0;                               -- record index = 0
706:     c.r[16] = c.r[20] + c.r[17];               -- search_key + offset (filename compare ptr)
...
708: L_001C00A8:                                    -- record walking loop
709:     c.r[4] = c.r[3] + c.r[18];                 -- base + record_index
710:     c.r[2] = ld32(c.r[16]);                    -- read 4 bytes from search key
711:     c.r[3] = c.r[4] + c.r[17];                 -- base + record_index + offset
712:     c.r[3] = ld32(c.r[3] + 48u);               -- read at offset +48 (next record boundary)
714:     if (c.r[3] == c.r[2]) goto L_001C00E8;     -- match found
...
716: L_001C00C0:                                    -- loop increment path
717:     c.r[19] = c.r[19] + 1;                     -- record counter +1
718:     c.r[2] = ld32(c.r[3] + 44u);               -- READ byte_size at offset +44 again
719:     if (c.r[19] < c.r[2]) goto L_001C00A8;     -- continue while counter < byte_size
721:     c.r[18] = c.r[18] + 48u;                   -- **STRIDE +48 (0x30 bytes per record)**
...
728: L_001C00E8:                                    -- match found
728:     c.r[4] = c.r[4] + 48u;                     -- advance past matched record
730:     call unit_0024(c, 83u, depth+1);           -- string comparison helper
...
734:     c.r[3] = ld32(c.r[28] - 10356);            -- base pointer again
735:     c.r[2] = c.r[19] << 1;                     -- index * 2
736:     c.r[2] = c.r[2] + c.r[19];                 -- index * 3
737:     c.r[2] = c.r[2] << 4;                      -- index * 48
738:     c.r[2] = c.r[2] + c.r[3];                  -- base + index * 48
739:     c.r[2] = ld32(c.r[2] + 92u);               -- READ at base + index*48 + 92
```

### Key Findings

| Aspect | Evidence | ELF Research Match |
|--------|----------|-------------------|
| Entry point | `unit_0030.cpp:681`, `L_001C0050` | ELF 0x1C0050 ✓ |
| Record stride | +48 bytes (line 721: `c.r[18] += 48u`) | 0x30-byte records ✓ |
| Byte size field | offset +44 (lines 701, 718: `ld32(base + 44u)`) | INFO.DAT record +0x2C = byte size ✓ |
| Filename comparison | unit_0024 entries 91u/83u (string compare) | Walks records matching names ✓ |
| Sector computation | lines 735-738: `index * 3 << 4 = index * 48` | Converts record index to record offset ✓ |
| Post-match read | offset +92 from base+index*48 (line 739) | Reads next field after record ✓ |

### Callers of L_001C0050

- `unit_0030.cpp:784` — `unit_0030(c, 11u)` called from `L_001C01BC` (self-recursive within unit_0030)
- This is entry 11 of unit_0030, dispatched from the unit_0030 switch table at line 151

## 4. String Comparison Helpers — unit_0024.cpp

The INFO.DAT lookup calls `unit_0024` with several entry points for string comparison:

| Entry | Label | Line | VA | Function |
|-------|-------|------|-----|----------|
| 71u | `L_00191000` | 1637 | `0x00191000` | String comparison (byte-by-byte with case handling) |
| 83u | `L_00191398` | 1893 | `0x00191398` | SIMD string comparison (16-byte chunks) |
| 84u | `L_00191410` | 1925 | `0x00191410` | SIMD string comparison |
| 87u | `L_001914E0` | 1986 | `0x001914E0` | String copy (byte-by-byte) |
| 91u | `L_001915F8` | 2062 | `0x001915F8` | SIMD string comparison |

These are called from `unit_0030.cpp` L_001C0050 at:
- Line 690: `unit_0024(c, 91u)` — likely gets INFO.DAT base or compares filenames
- Line 730: `unit_0024(c, 83u)` — string comparison after match

## 5. GAME.DAT Sector-Based Reading

### Sector-to-Byte Conversion (`<< 11` = multiply by 2048)

The `<< 11` shift (multiply by 2048 = sector size) appears across many units, consistent with GAME.DAT sector-based reading: `byte_offset = sector_offset * 2048`.

| Unit | Line | Code | Context |
|------|------|------|---------|
| unit_0008.cpp | 2789 | `c.r[3] << 11` | Sector-to-byte conversion |
| unit_0007.cpp | 822 | `c.r[18] << 11` | Sector-to-byte conversion |
| unit_000C.cpp | 1881 | `c.r[5] << 11` | Sector-to-byte conversion |
| unit_0006.cpp | 1718 | `c.r[4] << 11` | Sector-to-byte conversion |
| unit_0009.cpp | 1115 | `c.r[2] << 11` | Sector-to-byte conversion |
| unit_000A.cpp | 1257 | `c.r[6] << 11` | Sector-to-byte conversion |
| unit_0011.cpp | 3873 | `c.r[10] << 11` | Sector-to-byte conversion |
| unit_0012.cpp | 3820 | `c.r[2] << 11` | Sector-to-byte conversion |
| unit_0014.cpp | 1488 | `c.r[2] << 11` | Sector-to-byte conversion |
| unit_0016.cpp | 2791 | `c.r[2] << 11` | Sector-to-byte conversion |
| unit_0018.cpp | 4436 | `c.r[10] << 11` | Sector-to-byte conversion |
| unit_0019.cpp | 5052 | `c.r[2] << 11` | Sector-to-byte conversion |
| unit_0020.cpp | 1742 | `c.r[4] << 11` | Sector-to-byte conversion |
| unit_0021.cpp | 4352 | `c.r[17] << 11` | Sector-to-byte conversion |
| unit_0030.cpp | 2411 | `c.r[2] << 11` | Sector-to-byte conversion (calls unit_002F entry 392u) |
| unit_008B.cpp | 2847 | `c.r[21] << 11` | Sector-to-byte conversion |
| unit_00D3.cpp | 201 | `c.r[5] << 11` | Sector-to-byte conversion |
| unit_00DA.cpp | 456 | `c.r[20] << 11` | Sector-to-byte conversion |
| unit_00DB.cpp | 595 | `c.r[15] << 11` | Sector-to-byte conversion |
| unit_00DD.cpp | 389 | `c.r[11] << 11` | Sector-to-byte conversion |
| unit_00E0.cpp | 258 | `c.r[7] << 11` | Sector-to-byte conversion |
| unit_00E1.cpp | 754 | `c.r[8] << 11` | Sector-to-byte conversion |
| unit_00E7.cpp | 272 | `c.r[27] << 11` | Sector-to-byte conversion |
| unit_00E8.cpp | 551 | `c.r[22] << 11` | Sector-to-byte conversion |

### 0x005A0000 Base Address

Appears repeatedly in `unit_0030.cpp`:
- Line 801: `c.r[2] = 0x005A0000 + 27216`
- Line 805: `c.r[2] = 0x005A0000 + 23120`
- Line 811: `c.r[4] = 0x005A0000 + 21936`
- Line 818: `c.r[4] = 0x005A0000 + 21976`
- Line 826: `c.r[4] = 0x005A0000 + 22016`
- Line 834: `c.r[4] = 0x005A0000 + 22056`

This is likely a RAM base address for decoded INFO.DAT data or GAME.DAT sector buffer in EE RAM (0x005A0000 range).

## 6. Asset Loading by Stem Name (.stg, .mdl, .hit, .pix)

### Search Results

| Search term | Result |
|-------------|--------|
| `.stg` | No string literal matches |
| `.mdl` | No string literal matches |
| `.hit` | No string literal matches |
| `.pix` | No string literal matches |

No file extension string literals exist in the EE units. Asset loading by stem name works through:
1. **INFO.DAT table lookup** (`unit_0030.cpp` L_001C0050) — walks records matching the asset name
2. **String comparison** (`unit_0024.cpp` entries 71u/83u/84u/91u) — compares filenames without extensions
3. **Sector computation** — `index * 48` record offset, then `sector * 2048` for GAME.DAT byte offset

The asset name (without extension) is passed as `c.r[4]` to `L_001C0050`, compared against INFO.DAT record filenames using `unit_0024` string comparison, and on match the sector/byte_size fields are read to compute the GAME.DAT read position.

## 7. Nibble-Swap Codec

### Search Results

| Search term | Result |
|-------------|--------|
| `nibble` | No matches |
| `swap_nibble` | No matches |
| `arkd` / `ARKD` | No matches |
| `0x7F777777` (signature start) | No matches |
| `www.arika` | No matches |

### Conclusion

The nibble-swap + bit-inversion + key-subtraction codec does **not** exist in the EE units. Per the ELF research, it runs entirely on the IOP:
- ARKD_DVD module at IOP 0x67800
- Decoder routine at IOP 0x67AC0
- Nibble swaps at IOP 0x67B88/0x67B8C
- Bit inversion at IOP 0x67BBC
- Key subtraction at IOP 0x67BF4

The EE receives already-decoded INFO.DAT data (confirmed by research: decoded buffer at EE RAM 0x015CE400 and IOP RAM 0x000BE300).

## 8. Complete Call Chain

```
Caller (unknown unit)
  → unit_0030(c, 11u) [L_001C0050, line 681]  -- EE INFO.DAT lookup
    → unit_0024(c, 91u) [L_001915F8, line 2062]  -- string compare / base lookup
    → [walk 48-byte records, stride +48]
    → read byte_size at record+44
    → on match: unit_0024(c, 83u) [L_00191398, line 1893]  -- string compare
    → compute sector from record index (index * 48)
    → << 11 (sector * 2048) for GAME.DAT byte offset
```

## 9. Comparison: ELF Research vs. Generated Code

| Aspect | ELF Research | Generated EE Code |
|--------|-------------|-------------------|
| INFO.DAT decoder | IOP 0x67AC0 (ARKD_DVD) | NOT in EE (IOP-side) |
| Nibble-swap codec | IOP 0x67B88/0x67B8C | NOT in EE |
| EE record lookup | ELF 0x1C0050 | unit_0030.cpp:681 (L_001C0050) ✓ |
| Record size | 0x30 (48) bytes | +48 stride ✓ |
| Record fields | filename+32, unknown+32, sector+36, sectors+40, size+44 | +44 read ✓, +36 implied |
| Sector→byte | sector * 2048 | `<< 11` shifts ✓ |
| Decoded buffer | EE RAM 0x015CE400 | 0x005A0000 base in unit_0030.cpp |

## 10. Files Searched

All 249 files in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`:
- 240 EE units: `unit_0000.cpp` through `unit_00F8.cpp`
- 9 VU1 microcode units: `vu1_000.cpp` through `vu1_008.cpp`
- `table.cpp`, `units.hpp`
