# Recompiled Code — Audio Code Search Results

## 1. Summary

**No audio-related code exists in any of the 240 generated units** (`unit_0000.cpp` through `unit_00F8.cpp`) in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`. This is expected behavior for a PS2 game decompilation: audio on the PS2 was handled by the **IOP (Input/Output Processor)**, not the EE (Emotion Engine). The EE would send SIF commands to the IOP to trigger sounds, but all actual audio code (SPU2 register access, ADPCM decoding, voice allocation, sound bank management, music streaming) ran on the IOP and is not part of the EE code that was decompiled/recompiled.

## 2. Unit Address Mapping (from table.cpp)

Each unit covers a 16KB VA range:

| Unit | VA Range | Unit | VA Range | Unit | VA Range |
|------|----------|------|----------|------|----------|
| unit_0000 | 0x00100000 | unit_0040 | 0x00200000 | unit_0080 | 0x00300000 |
| unit_0001 | 0x00104000 | unit_0041 | 0x00204000 | unit_0081 | 0x00304000 |
| unit_0002 | 0x00108000 | unit_0042 | 0x00208000 | unit_0082 | 0x00308000 |
| unit_0003 | 0x0010C000 | unit_0043 | 0x0020C000 | unit_0083 | 0x0030C000 |
| unit_0004 | 0x00110000 | unit_0044 | 0x00210000 | unit_0084 | 0x00310000 |
| unit_0005 | 0x00114000 | unit_0045 | 0x00214000 | unit_0085 | 0x00314000 |
| unit_0006 | 0x00118000 | unit_0046 | 0x00218000 | unit_0086 | 0x00318000 |
| unit_0007 | 0x0011C000 | unit_0047 | 0x0021C000 | unit_0087 | 0x0031C000 |
| unit_0008 | 0x00120000 | unit_0048 | 0x00220000 | unit_0088 | 0x00320000 |
| unit_0009 | 0x00124000 | unit_0049 | 0x00224000 | unit_0089 | 0x00324000 |
| unit_000A | 0x00128000 | unit_004A | 0x00228000 | unit_008A | 0x00328000 |
| unit_000B | 0x0012C000 | unit_004B | 0x0022C000 | unit_008B | 0x0032C000 |
| unit_000C | 0x00130000 | unit_004C | 0x00230000 | unit_008C | 0x00330000 |
| unit_000D | 0x00134000 | unit_004D | 0x00234000 | unit_008D | 0x00334000 |
| unit_000E | 0x00138000 | unit_004E | 0x00238000 | unit_008E | 0x00338000 |
| unit_000F | 0x0013C000 | unit_004F | 0x0023C000 | unit_008F | 0x0033C000 |
| unit_0010 | 0x00140000 | unit_0050 | 0x00240000 | unit_0090 | 0x00340000 |
| ... | ... | ... | ... | ... | ... |
| unit_00DF | 0x0047C000 | unit_00E0 | 0x00480000 | unit_00F8 | 0x004E0000 |

Total: 240 units covering VA `0x00100000`–`0x004F0000` (with gaps: no units for 0x0024D000–0x0024F000, 0x0044C000–0x0044F000, etc.).

## 3. Search Results by Audio Category

### 3.1 SPU2 References — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bspu\b`, `\bSPU\b` | No matches |
| `SPU2` | No matches |
| `0x1F400000` (SPU2 register base) | No matches |
| `0x1F800000` (SPU register base) | No matches |

### 3.2 ADPCM Decoding — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\badpcm\b`, `\bADPCM\b` | No matches |
| `\bpredictor\b` | No matches |
| `\bnibble\b` | No matches |

### 3.3 ASE Sound Banks / Sample Playback — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bass\b` | No matches |
| `\bsound_bank\b` | No matches |
| `\bsample_playback\b` | No matches |
| `\bvoice_alloc\b` | No matches |

### 3.4 AMS Music Streams — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bams\b` | No matches |
| `\bmusic_stream\b` | No matches |
| `\bintro_loop\b` | No matches |
| `_in\b`, `_lp\b` | Only matched as part of `case` labels (false positives) |

### 3.5 Audio Triggers on Game Events — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bplay_sound\b` | No matches |
| `\bsound\b` (whole word) | No matches |
| `\baudio\b` (whole word) | No matches |
| `\bvoice\b` (whole word) | No matches |
| `\bsample\b` (whole word) | No matches |
| `\bmusic\b` (whole word) | No matches |
| `\btrigger\b` (whole word) | No matches |
| `\bdecode\b` (whole word) | No matches |
| `\bstream\b` (whole word) | No matches |
| `\bbank\b` (whole word) | No matches |
| `\bplay\b` (whole word) | No matches |
| `\bvolume\b` (whole word) | No matches |
| `\bpitch\b` (whole word) | No matches |
| `\bfreq\b` (whole word) | No matches |

### 3.6 Sound File Extensions — NOT FOUND

| Search term | Result |
|-------------|--------|
| `.VAG`, `.vag` | No matches |
| `.SE`, `.se` | No matches |
| `.SND`, `.snd` | No matches |
| `.SAM`, `.sam` | No matches |
| `.ADX`, `.adx` | No matches |

### 3.7 SIF/IOP Communication — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bSIF\b`, `\bsif\b` | No matches |
| `\bIOP\b` | No matches |

### 3.8 `0x1F000000` — FALSE POSITIVE (VU/COP2, not audio)

Found in `unit_001C.cpp` at lines 2815, 2823, 2985, but these are VU/COP2 vector operations (using `cfc2` COP2 register read and bitmask operations with `0x3C00`/`15360`), not SPU2 audio register access.

## 4. Why No Audio Code Exists

On the PlayStation 2 architecture:

- **EE (Emotion Engine)** — main CPU, runs game logic, physics, rendering. This is what was decompiled into the 240 generated units.
- **IOP (Input/Output Processor)** — secondary CPU, handles peripherals, audio, CD/DVD access. Audio code runs here.
- Communication between EE and IOP happens via **SIF** (Streaming Interface) commands.

The EE game code would send SIF commands like "play sound X" or "load bank Y" to the IOP, but the actual audio implementation (SPU2 register programming, ADPCM decoding, voice allocation, mixing, music streaming) is entirely on the IOP side and is **not present** in the EE code that was decompiled.

The `ps2-web-recomp` project only decompiles/recompiles the EE code, so audio is outside its scope.

## 5. Implications for Future Work

If audio research is needed:

1. **IOP code** — The PS2 IOP (also called "Iop" or "SIO2") code would need to be decompiled separately. This is typically in a different binary region (often at IOP address space `0x00000000`–`0x00200000`).
2. **SIF command tracing** — Trace SIF command sends from EE to IOP to identify which sound IDs/banks are triggered by game events (spawn, hits, menu navigation, etc.).
3. **SPU2 register documentation** — SPU2 registers at `0x1F400000`–`0x1F400FFF` on the PS2.
4. **Sound bank files** — Look for `.SE`, `.VAG`, `.SND` files in the game ISO; ASE banks and ADPCM-encoded samples would be in these.
5. **EE-side audio triggers** — Look for SIF-related function calls in the EE code (none found via `\bSIF\b` search, suggesting SIF may be wrapped in a library function with a different name, or the SIF calls may be inlined differently in the recompiled code).

## 6. Files Searched

All 240 `.cpp` files in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`:
- `unit_0000.cpp` through `unit_00BF.cpp` (0x00100000–0x003FC000)
- `unit_00C0.cpp` through `unit_00F8.cpp` (0x00400000–0x004E0000)
- `table.cpp` (unit address mapping, no audio code)
