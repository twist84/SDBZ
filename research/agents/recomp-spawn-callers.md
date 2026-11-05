# Recompiled Spawn Routine — Caller Chain Analysis

## 1. Overview

The spawn routine lives in `unit_004E.cpp` (VA `[0x00238000, 0x0023C000)`) with four entry points:

| Entry | Label | Line | VA | Role |
|-------|-------|------|-----|------|
| 319 | `L_0023A430` | 3367 | `0x0023A430` | Pre-spawn object resolver |
| 399 | `L_0023AC90` | 3972 | `0x0023AC90` | Placement body (contains SLES `0x23ACC8`) |
| 405 | `L_0023B020` | 4210 | `0x0023B020` | Alternate entry |
| 407 | `L_0023B0E0` | 4258 | `0x0023B0E0` | Alternate entry (versus screen) |
| 411 | `L_0023B140` | 4286 | `0x0023B140` | Main spawn sub-routine entry (14 callers) |

Dispatch mapping (unit_004E.cpp dispatch table):
- `case 319u: goto L_0023A430;` — pre-spawn resolver
- `case 399u: goto L_0023AC90;` — placement body
- `case 405u: goto L_0023B020;` — alternate entry
- `case 407u: goto L_0023B0E0;` — alternate entry (versus screen)
- `case 411u: goto L_0023B140;` — main spawn entry

Callers use the `unit_004E(c, entry, depth+1)` convention. `$a0` = `c.r[4]`, `$a1` = `c.r[5]`.

## 2. Caller Units and Entry Points

### 2.1 Entry 411 callers — main spawn wrapper (with pre-spawn resolver entry 319)

These 15 call sites across 14 caller units follow the paired pattern: first `unit_004E(c, 319u)` (pre-spawn resolver), then `unit_004E(c, 411u)` (spawn). Corresponds to SLUS spawn callers at `0x23B140`.

| # | Unit | Line (entry 319) | Line (entry 411) | SLES return PC | SLUS VA (research) | Lifecycle context |
|---|------|------------------|------------------|----------------|--------------------|-------------------|
| 1 | unit_0050.cpp | 2491 | 2494 | `0x00241B44` / `0x00241B4C` | `0x00251F50` | Fight init / stage load |
| 2 | unit_0054.cpp | 2734 | 2737 | `0x0025297C` / `0x00252984` | `0x0025297C` | Rematch |
| 3 | unit_0058.cpp | 3487 | 3490 | `0x00262AC8` / `0x00262AD0` | `0x00262AC8` | Fight init / stage load |
| 4 | unit_0056.cpp | 2500 | 2503 | `0x00259B24` / `0x00259B2C` | `0x00259B24` | Round reset |
| 5 | unit_0056.cpp | 3066 | 3069 | `0x0025A3A4` / `0x0025A3AC` | `0x0025A3A4` | Stage transition |
| 6 | unit_0055.cpp | 2863 | — | `0x00256048` | `0x00256048` | Fight init (variant=1) |
| 7 | unit_0055.cpp | 3421 | — | `0x0025686C` | `0x0025686C` | Fight init (variant=0) |
| 8 | unit_005C.cpp | 1208 | 1211 | `0x00270798` / `0x002707A0` | `0x00270798` | Rematch |
| 9 | unit_005E.cpp | 2113 | 2116 | `0x00279638` / `0x00279640` | `0x00279638` | Fight init / stage load |
| 10 | unit_0062.cpp | 1278 | 1281 | `0x00288A04` / `0x00288A0C` | `0x00288A04` | Fight init / stage load |
| 11 | unit_0061.cpp | 4428 | 4431 | `0x002876C0` / `0x002876C8` | `0x002876C0` | Fight init / stage load |
| 12 | unit_005F.cpp | 3893 | — | `0x0027EF98` | `0x0027EF98` | Fight init (variant=1) |
| 13 | unit_0066.cpp | 3372 | 3375 | `0x0029AAA8` / `0x0029AAB0` | `0x0029AAA8` | Round reset |
| 14 | unit_0067.cpp | 4062 | 4065 | `0x0029F2D8` / `0x0029F2E0` | `0x0029F2D8` | Stage transition |
| 15 | unit_0065.cpp | 3844 | 3847 | `0x00296E7C` / `0x00296E84` | `0x00296E7C` | Rematch |

SLUS lifecycle mapping (from research `mips-disassembly-analysis.md` §2.8 / §2.11):
- `0x25297C`, `0x270798`, `0x296E7C` → **Rematch** (re-placement after match end)
- `0x259B24`, `0x29AAA8` → **Round reset** (re-placement between rounds)
- `0x25A3A4`, `0x29F2D8` → **Stage transition** (stage load / transition)
- `0x262AC8`, `0x288A04`, `0x2876C0` → **Fight init** (initial stage load)
- `0x298180` (SLUS) → **Session / character-select → fight transition** (loop gated on `fighter+0x528 == 29`)

### 2.2 Entry 399 callers — placement body (direct, no pre-spawn resolver)

These callers pass stage (`$a0 = *(session + 0x58)`) and fighter (`$a1`) directly to `L_0023AC90`, bypassing `L_0023A430`. Called **in pairs** for two fighters (P1 at `session+0x60`, P2 at `session+0x64`).

| Unit | Lines | SLES return PC | SLUS VA | Call pattern |
|------|-------|----------------|---------|-------------|
| unit_0065.cpp | 4296, 4300 | `0x002974F4`, `0x00297500` | — | $a0=c.r[19], $a1=c.r[18] then $a1=c.r[17] (two fighters) |
| unit_00A4.cpp | 2777, 2781 | `0x00391CFC`, `0x00391D08` | `0x0039xxxx` | $a0=*(session+0x58), $a1=*(session+0x60) then *(session+0x64) |
| unit_00A5.cpp | 1200, 1204, 4893, 4897 | `0x00394640`, `0x0039464C`, `0x00397890`, `0x0039789C` | — | Same pattern, $a0=*(session+0x58) |
| unit_00A6.cpp | 1237, 1241, 3572, 3576 | `0x003985F8`, `0x00398604`, `0x0039A4A4`, `0x0039A4B0` | — | Same pattern |
| unit_00A7.cpp | 1753, 1757, 4262, 4266 | `0x0039CCCC`, `0x0039CCD8`, `0x0039ED98`, `0x0039EDA4` | — | Same pattern |
| unit_00B8.cpp | 2556, 2562 | `0x003E1840`, `0x003E1854` | — | $a0=c.r[18], $a1=c.r[18] then c.r[17] |
| unit_00BA.cpp | 1830, 1834 | `0x003E8D14`, `0x003E8D20` | — | $a0=*(c.r[16]+104), $a1=c.r[17] |
| unit_00BB.cpp | 3718 | `0x003EEA50` | — | $a0=*(c.r[16]+104), $a1=c.r[17] |
| unit_00C0.cpp | 4114, 4118 | `0x00402B68`, `0x00402B74` | — | $a0=c.r[18], $a1=c.r[18] then c.r[17] |
| unit_00C1.cpp | 5115, 5121 | `0x004079A0`, `0x004079B4` | — | $a1=*(sp+40), $a1=*(c.r[16]) |

These SLUS addresses (`0x39xxxx` range: `0x392D80`, `0x3956A0`, `0x398910`, `0x399640`, `0x39B550`, `0x39DD40`, `0x39FE00`) match the research's fight-init / rematch caller list.

### 2.3 Entry 405 caller — alternate spawn entry

| Unit | Line | SLES return PC | Entry | Notes |
|------|------|----------------|-------|-------|
| unit_00BB.cpp | 4142 | `0x003EEFB4` | 405 (`L_0023B020`) | $a0 = c.r[21] |

### 2.4 Entry 407 caller — versus screen (lightweight entry)

| Unit | Line | SLES return PC | SLUS VA | Entry | Notes |
|------|------|----------------|---------|-------|-------|
| unit_006B.cpp | 1991 | `0x002AD2DC` | `0x002AD2D4` | 407 (`L_0023B0E0`) | **Versus screen** — matches research's "versus screen caller at 0x2AD2D4 via lightweight 0x23B0E0 entry". $a1 = *(c.r[17]) |

### 2.5 Entry 319 callers without entry 411 follow-up

| Unit | Line | SLES return PC | Notes |
|------|------|----------------|-------|
| unit_004F.cpp | 4857 | `0x0023F87C` | Pre-spawn resolver only, $a1 = 0 |

## 3. Argument Resolution: Stage (+0x58) and Fighter (+0x60/+0x64)

### 3.1 Entry 399 callers — direct stage/fighter resolution

These callers resolve stage and fighter directly from the session/owner object before calling `L_0023AC90`:

**unit_00A4.cpp:2775-2781** (representative):
```
set32(c.r[5], ld32((c.r[16].w[0] + 96u)));   // $a1 = *(session + 0x60) = fighter #1 (P1)
set32(c.r[4], ld32((c.r[16].w[0] + 88u)));   // $a0 = *(session + 0x58) = stage
unit_004E(c, 399u, ...);                      // call L_0023AC90(stage, fighter1)
set32(c.r[5], ld32((c.r[16].w[0] + 100u)));  // $a1 = *(session + 0x64) = fighter #2 (P2)
set32(c.r[4], ld32((c.r[16].w[0] + 88u)));   // $a0 = *(session + 0x58) = stage (same)
unit_004E(c, 399u, ...);                      // call L_0023AC90(stage, fighter2)
```

This exactly matches the research's documented calling convention:
- `stage = session + 0x58`
- `fighter #1 = session + 0x60`
- `fighter #2 = session + 0x64`
- Called in pairs for two fighters

**unit_0065.cpp:4294-4300** (same pattern, different registers):
```
c.r[4] = c.r[19];              // $a0 = c.r[19] (stage)
c.r[5] = c.r[18];              // $a1 = c.r[18] (fighter #1)
unit_004E(c, 399u, ...);
c.r[4] = c.r[19];              // $a0 = c.r[19] (same stage)
c.r[5] = c.r[17];              // $a1 = c.r[17] (fighter #2)
unit_004E(c, 399u, ...);
```

### 3.2 Entry 411 callers — pre-spawn resolver + spawn wrapper

These callers use the two-step pattern documented in the research:

**Step 1 — `L_0023A430` (entry 319, pre-spawn resolver)** at unit_004E.cpp:3367:
```
st32((c.r[4].w[0] + 44u), c.r[5].w[0]);   // *(session + 0x2C) = $a1 (fighter pointer)
c.r[3] = 0x10000;
set32(c.r[5], ld32((c.r[4].w[0] + 4u)));   // c.r[5] = *(session + 4)
c.r[3] = c.r[5] | c.r[3];                   // c.r[3] = *(session + 4) | 0x10000
st32((c.r[4].w[0] + 4u), c.r[3].w[0]);     // *(session + 4) = c.r[3] (set flag)
```
- `$a0` = session/owner object, `$a1` = fighter pointer
- Stores fighter at `session + 0x2C` (44 decimal)
- Sets `session + 4` flag bit `0x10000`

**Step 2 — `L_0023B140` (entry 411, spawn wrapper)** at unit_004E.cpp:4286:
```
set32(c.r[5], ld32((c.r[4].w[0] + 4u)));   // $a1 = *(session + 4)
c.r[3] = c.r[5] & 0x10000;                   // check flag bit
if (c.r[3] == 0) goto L_0023B728;           // if flag not set, alternate path
c.r[20] = c.r[4];                           // save session pointer
set32(c.r[5], ld32((c.r[20].w[0] + 44u)));  // $a1 = *(session + 0x2C) = fighter (loaded back)
st32((c.r[20].w[0] + 36u), c.r[5].w[0]);    // *(session + 0x24) = fighter (field_24 setup)
```

### 3.3 Variant of argument setup

Some entry 411 callers set `$a1 = 0` explicitly before `L_0023A430` (e.g., unit_0050.cpp:2487, unit_0054.cpp:2734 sets `$a1` from prior call result), while others set `$a1 = 1` (e.g., unit_0055.cpp:2863 `set32(c.r[5], 0u + 1u)`). This `$a1` becomes the value stored at `session + 0x2C` by `L_0023A430`, and is later used by `L_0023B140` as the fighter pointer — see §4 for the field_24 implication.

## 4. field_08 (+0x08) and field_24 (+0x24) Setup by Callers

### 4.1 Entry 399 callers — no caller-side setup

Entry 399 callers pass the stage object directly as `$a0`. `L_0023AC90` (unit_004E.cpp:3987-3993) reads `field_08` from `stage+8` and `field_24` from `stage+36` internally:
```
c.r[5] = *(stage + 8);      // field_08 (stage ID 0-12)
c.r[2] = *(stage + 36);     // field_24 (variant bit 0/1)
```
No caller modifies `field_08` or `field_24` before calling entry 399. They are whatever values exist in the stage object.

### 4.2 Entry 411 callers — variant bit set via L_0023A430's $a1

`L_0023B140` (unit_004E.cpp:4301-4302) copies `session + 0x2C` to `session + 0x24`:
```
c.r[5] = *(session + 0x2C);   // load value stored by L_0023A430
*(session + 0x24) = c.r[5];   // store to field_24 location
```

The value at `session + 0x2C` was set by `L_0023A430` from `$a1`. Different callers pass different `$a1`:
- `$a1 = 0`: unit_0050.cpp:2487, unit_0054.cpp:2734 (from prior call), unit_0055.cpp:3421 → **field_24 = 0** (variant 0)
- `$a1 = 1`: unit_0055.cpp:2863 (`set32(c.r[5], 0u + 1u)`), unit_005F.cpp:3893 (`set32(c.r[5], 0u + 1u)`) → **field_24 = 1** (variant 1)

This means **unit_0055.cpp** calls `L_0023A430` twice — once with `$a1 = 1` (line 2863) and once with `$a1 = 0` (line 3421) — setting up **different field_24 variants for two fighters**. This matches the research's finding that `field_24` is the stage variant/sub-stage bit (0/1), and variant matters for stage IDs 3, 4, 5 (map entries `field_08*2` vs `field_08*2+1`).

## 5. Summary of Caller → Lifecycle Mapping

| SLUS VA range | Count | Lifecycle | Entry points used |
|---------------|-------|-----------|-------------------|
| `0x25xxxx`–`0x29xxxx` | ~15 | Fight init / stage load / rematch / round reset / stage transition | Entry 319 + Entry 411 |
| `0x39xxxx` | ~10 | Fight init / rematch | Entry 399 (direct, paired) |
| `0x2AD2D4` | 1 | Versus screen | Entry 407 (lightweight) |
| `0x3EEFB4` | 1 | ? | Entry 405 (alternate) |

The two calling conventions (entry 411 wrapper vs entry 399 direct) represent the same spawn logic reached through different code paths in the SLES build. Entry 399 callers resolve stage/fighter directly from `session+0x58/0x60/0x64`; entry 411 callers go through `L_0023A430` which stores the fighter at `session+0x2C` and sets a flag at `session+4`, then `L_0023B140` validates the flag and loads the fighter back.
