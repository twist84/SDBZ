# Super Dragon Ball Z — Research Report: Systems Beyond Spawn Mechanics

## 1. Scope of Analysis

Explored the `other-systems` worktree at `C:\Dev\SuperDragonBallZ\.kilo\worktrees\other-systems` via subagent exploration plus direct directory reads. The project is a C++23 reconstruction of Super Dragon Ball Z (PS2, 2005) using CMake/SDL3/OpenGL. It is currently a **model/stage viewer**, not a full fighting game. Combat, character selection, save data, and AI are planned but unimplemented.

## 2. ELF Structure

### ELF Files Present
**None.** The repository contains no ELF binary (SLUS_214.42 or SLES_541.61). Only an ELF32 header parser exists.

### ELF Parser (`source/core/elf.hpp` / `source/core/elf.cpp`)
- `inspect_elf32(std::span<const std::uint8_t> data)` validates ELF32 headers:
  - Magic `0x7F 'E' 'L' 'F'`, 32-bit class, little-endian, EV_CURRENT version
  - Reads type, machine, entry point, program/section header offsets/counts/sizes, flags
  - Validates header size == 52, table bounds within file
- `elf_machine_name(uint16_t machine)` — MIPS (8), i386 (3), ARM (40), x86-64 (62), AArch64 (183)
- PS2 detection in `source/tools/main.cpp` (`command_elf`): Machine == 8 (MIPS) AND `(flags & 0xF0000000) == 0x20000000`
- Tool command: `sdbz_tool elf <SLUS_214.42>` prints header fields

### Disassembly-Documented Addresses (SLUS_214.42 / USA)
From `research/format_interaction_report.md`:

| Address | Description |
|---------|-------------|
| `0x003301E0-0x00330300` | CEBZ effect loading code |
| `0x00191EF8` | String hash function (effect name lookup) |
| `0x00191638` | Data unpacking (.ebz entry parsing) |
| `0x004CEF28` | `"%s.ebz"` format string |
| `0x004C3AF0` | `"eff/e%c%c/%s"` path format |
| `0x004CC5B0` | `"%s.pix"` format string |
| `0x004CC5A8` | `"%s.mdl"` format string |
| `0x004CC728` | `"%s.pmt"` format string |
| `0x004CFAF0` | `"%s.ani"` format string |
| `0x004CFAC8` | `"%s.pas"` format string |
| `0x004CA6B8` | `"%s.cht"` format string |
| `0x004CD460` | `"font/5-24-2.fon"` string |
| `0x004CA870` | CEffectMgr class name |
| `0x004CEF18` | CEBZData class name |
| `0x004CEF50` | CEBZEffectObj class name |
| `0x004FE628` | CEBZ class info block |
| `0x004DB060-0x004DB740` | 25 SPT script file paths |
| `0x1A0244-0x1A0300` | Quaternion half-angle formula (EE RAM) |
| `0x1A0100` | Node creation (EE RAM) |
| `0x1B12F0` | MOT2 loader (EE RAM) |
| `0x1B2C80` | Quaternion slerp with shortest-path fallback (EE RAM) |
| `0x01784C00` | s06a.hit data in EE RAM |
| `0x0179AC00` | STG name table + node tags (EE RAM) |
| `0x015CE400` / `0x000BE300` | Decoded INFO.DAT in EE/IOP RAM |

### INFO.DAT Decoder (`research/archive_and_container_notes.md`)
- Decoder at `0x67AC0` (module RVA `0x2C0`)
- Nibble swaps at `0x67B88`/`0x67B8C`, inversion at `0x67BBC`, key subtraction at `0x67BF4`, table pointer at `0x72B04`
- EE lookup at `0x1C0050` walks 0x30-byte records
- Decoded INFO.DAT matches EE RAM at `0x015CE400` and IOP RAM at `0x000BE300` in `stage_stall.bin` snapshot

### Binary Analysis Tools
`file`, `readelf`, `objdump`, `strings` not available in environment. No ELF binary to analyze anyway.

## 3. Stages

### STG Format (`source/core/stage.hpp` / `stage.cpp`)
**Header** (version `0x000100`, magic `"STG "`):
- Offset 0: u32 version
- Offset 4: magic `"STG "` (4 bytes)
- Offset 8: u32 name count
- Offset 12: u32 template root count
- Offset 16: fixed 16-byte CP932 names (name_count entries)
- Variable: node records follow names

**Node record** (base 44 bytes; kinds 4/6 = 60 bytes; kind 5 = 96 bytes):
- +0: kind (u8), inline reference count (u8), links (u8), flags (u8)
- +4: name index (u32)
- +8–+43: translation (3 floats), rotation radians (3 floats), scale (3 floats)
- +44+: inline name-index/raw-flags pairs (8 bytes each, kind 3 only)

**Hierarchy:** link bit 0 = child, bit 1 = sibling. Roots = `template_count + 2`: template roots, then EFFECT root, then STAGE root. Final roots must be named "EFFECT" and "STAGE".

**Parser validation:** kind 1–6, links < 4, name index in range, inline references only on kind 3, root count matches, no truncated records, no unfulfilled links.

### HIT Format (`source/core/stage_collision.hpp` / `stage_collision.cpp`)
**Header** (version `0x00020000`, magic `"HIT "`):
- Offset 0: u32 version
- Offset 4: magic `"HIT "`
- Offset 8: object count (u32)
- Offset 12: objects offset (u32)
- Offset 16: polygons offset (u32)
- Offset 20: vertices offset (u32)

**HIT objects** (64 bytes each): name (16 bytes), flags, polygon count, first polygon, radius, 3 float4 vectors.

**HIT polygons** (32 bytes each): vertex count, first vertex, 2 unknown words, float4 normal.

**HIT vertices** (16 bytes each): float4 position.

### Stage Loading & Rendering
- **Disk** (`source/platform/sdl_main.cpp`): command-line `.stg` argument loads all `.stg` files in same folder, plus sibling `.mdl`, `.hit`, `.pix` companions by stem matching.
- **Archive VFS**: VFS browser selects `.stg` entries from INFO.DAT/GAME.DAT, loads companions from archive by stem name.
- **Rendering** (`source/render/stage_renderer.cpp`): renders kind-2 nodes (mesh instances) under STAGE root; groups 0=STAGE (always visible), 1=EFFECT (toggle E), 2=templates (toggle T); opaque/transparent alpha sorting (GL_GREATER 0.5); HIT wireframe debug (H key); stage auto-framing from kind-2 mesh vertex bounds.

### Negative Finding
No actual `.stg`/`.hit` files are included. `assets/` directory is empty per `.gitignore`.

## 4. Characters

### FMDL Model Format (`source/core/model.hpp` / `model.cpp`)
**Container** (from `research/archive_and_container_notes.md`):
- Magic `FMDL`, u32 total size, u32 chunk count, u32 format (0x100), 16 reserved bytes, chunk offset table
- Chunks: DMOD (geometry directory), DMAT (materials), XTEX, XMON (mesh names), XCUD, XMTX (hierarchy palette), DHIE (hierarchy directory), XNON (node names)

**DHIE/XHIE node records** (0x40 bytes each):
- 0x00: i32 node index
- 0x04: i32 parent index (-1 = root)
- 0x08: i32 first child index
- 0x0C: i32 next sibling index
- 0x10: float4 local scale XYZ
- 0x20: float4 local Euler degrees XYZ
- 0x30: float4 local translation XYZ

**XMOD geometry:** DMA tags at +0x1A0, vertex count at +0x30, triangle count at +0x34. Vertex flags at +0x0C (copied at +0x20). Vertex strides vary by flags:
- Flags 0/1: 48 bytes (position, normal, STQ)
- Flags 2/3: 80 bytes (position, normal, STQ, palette addresses, weights)
- Flag 4: 64 bytes (position, normal, color, STQ)
- Flag 6: 96 bytes (position, normal, color, STQ, palette addresses, weights)
- Flag 11: 80 bytes (position, normal, STQ, second position, second normal — morph targets)

**In-memory `s_model`:** nodes (hierarchy with parent/child/sibling, name, scale, rotation_degrees, translation), materials (colour_a float4, colour_b float4, texture_index i32), meshes (name, vertex_flags, vertices, primitives, rigid_node_index, is_skinned, has_morph).

### Animation Format (`source/core/animation.hpp` / `animation.cpp`)
**RFSM/L3 package:**
- Magic `RFSM` + `L3`, u16 motion count, u32 shared block offset, u32 shared block end
- 32-byte directory records: 16-byte CP932 name, absolute motion offset, motion size, unknown u32, format value (0x10006)
- Shared block: u16 motion count, u16 bone count, u32 bone names offset, u32 variable binding offset, u32 bone flags offset; bone names are fixed 16-byte records; one flag byte per bone

**MOT2 motion:**
- Magic `MOT2`, u32 total size, float FPS (60), u16 frame count, u16 motion-node count, u16 constant float4 count, u16 total channel count
- Offsets: node descriptor table, constant float4 pool, then 3 channel groups (translation, rotation, scale)
- Channel group: u16 channel_count, u16 key_count, u32 vectors_offset, u32 times_offset
- Node descriptor (8 bytes): u16 flags (bits 1/2/4 = T/R/S animated), u16 translation_index, u16 rotation_index, u16 scale_index
- Translation = linear interpolation; Rotation = shortest-path slerp with near-parallel linear fallback

### Animation State Classification
`source/gameplay/player_animation.hpp/.cpp` maps PMT clip names to states (idle/walk/run/jump/fall/land) by name matching.

### Negative Findings
- **Character selection:** Not implemented (planned Phase 7 per `plan.md`)
- **Move lists / frame data:** None present — animation clips classified by name only; no attack data, hitboxes, combo info
- **No actual character model files** (`.mdl`, `.pmt`, `.mot`) included

## 5. Game Systems

### Menus & UI (`source/interface/`)
- **HUD** (`hud.cpp`): displays title "INFO.DAT/GAME.DAT viewer", current model/stage/motion names, mesh filters, player controller status (position, velocity, grounded, HIT object/polygon), part group cycling (1–9 keys), stage toggles (E/H/T/F), camera info (perspective/ortho, yaw, pitch, distance, target).
- **Debug menu** (`debug_menu.cpp`, Home key toggle): stage cycle, motion cycle, show effect/collision/templates toggles, archive VFS, fit camera, quit. Navigation via D-pad/arrows, left/right adjusts values, Enter/Space activates, 0–9/a-z quick select.
- **VFS Overlay** (`vfs_overlay.cpp`): archive browser for INFO.DAT/GAME.DAT entries; breadcrumb path, file/dir list with markers; A/Enter opens dir or loads file; B/Backspace to parent; Esc closes. Loadable extensions: `.mdl`, `.pix`, `.fon`, `.pmt/.mot`, `.stg`, `.hit`.

### Combat System
**Not implemented.** `plan.md` Phase 5 describes planned combat subsystems (movement, hit detection, damage, defence, special moves, character states, resources, AI, camera, round manager), but none exist in code.

### Physics (`source/gameplay/player_controller.hpp/.cpp`, `source/core/stage_collision.hpp/.cpp`)
**Player controller** (`s_player_controller`):
- Position, velocity, ground_normal, ground_polygon_index, ground_object_name, yaw_degrees, ground_height
- walk_speed = 8.0, run_speed = 14.0, jump_speed = 7.5, gravity = 28.0
- acceleration = 36.0, deceleration = 42.0
- collision_capsule_radius = 0.25, collision_capsule_height = 1.75
- collision_step_height = 0.42, collision_step_down = 0.65
- max_walkable_slope_degrees = 60.0
- Fixed timestep: 60 Hz (`k_fixed_step = 1/60`), bounded accumulator (max 0.25s backlog)
- Camera-relative input rotation, jump edge buffering, ground support queried at destination

**Stage collision** (`s_stage_collision_world`):
- Built from STG kind-3 nodes referencing named HIT objects
- Vertices transformed by node world matrices (T*Rz*Ry*Rx*S)
- Newell's method for polygon normals (arbitrary planar n-gons)
- 8-unit XZ spatial grid broad-phase; large polygons (>4096 cells) use global fallback
- Ground query: walkable up to 60°, step up 0.35, step down 2.0
- Capsule support: centreline contact + footprint ring sampling (12 inner + 24 outer samples)
- Wall blocking/sliding: near-vertical polygons block horizontal movement, tangential preserved
- Overhead clearance: walkable polygons intersecting capsule volume rejected
- Edge falls: player falls when leaving valid support

### Audio (`source/core/ams.hpp/.cpp`, `ase.hpp/.cpp`, `audio/adpcm.hpp/.cpp`, `platform/audio.hpp/.cpp`)
**ASE sound banks** (version 100):
- 0x800-byte header, version=100, sample_count, payload_byte_count
- Repeated pitch/offset pairs at 0x0C
- Sony PS ADPCM (16 bytes → 28 signed samples)
- Sample flags: 4 at start, 3 at end

**AMS music streams** (version 120):
- 0x800-byte header, version=120, pitch, valid_bytes_per_channel, unit_count
- Each unit: 0x20000 bytes = two 0x10000-byte stereo blocks
- Play even blocks ascending, then odd blocks descending
- 0x8000 left + 0x8000 right per block
- `_in` and `_lp` suffixes distinguish intro/loop segments

**ADPCM decoder** (`s_adpcm_decoder`):
- 5 predictor coefficients: {0,0}, {60,0}, {115,-52}, {98,-55}, {122,-60}
- 4-bit nibbles, shift-based scaling, history1/history2 state
- Clamped to int16 range

**Platform audio** (`c_audio_device`): SDL3 S16, 44100 Hz, mono; `play_buffer()` is a **stub** — no audio playback.

### Inputs (`source/platform/event.hpp/.cpp`, `sdl_main.cpp`)
- **Keyboard:** WASD = camera-relative movement, Shift = run, Space = jump, P = toggle player control, C = re-centre camera, H = toggle HIT/capsule diagnostics
- **Mouse:** left drag = orbit, right/middle drag = pan, wheel = zoom
- **Gamepad:** SDL3, axis deadzone 0.18; left stick = move, left shoulder = run, A/South = jump, Select+Start = debug menu toggle, D-pad = menu navigation, B/East = back

### Save Data
**Not present.** Planned Phase 8 per `plan.md`. No save file format, save manager, or persistence code.

## 6. EU Beta Build Update Impact

The build is now **EU beta (SLES_541.61)**, not USA release (SLUS_214.42).

### What changes
| Area | Impact |
|------|--------|
| ELF disassembly addresses | **All SLUS_214.42 addresses likely shifted** in SLES_541.61. Must be re-verified against EU beta ELF: CEBZ loading, string hash, data unpacker, all format strings, CEffectMgr, CEBZ class info block, STG/HIT key addresses, SPT script paths, quaternion/MOT2/slerp addresses, INFO.DAT decoder, EE lookup. |
| STG/HIT/FMDL/MOT2 formats | **Likely unchanged** — on-disk binary structures are typically region-consistent. These findings remain valid. |
| INFO.DAT/GAME.DAT archive | `research/archive_and_container_notes.md` was already tested against SLES_541.61 — aligns with EU beta. However, actual record contents/entry names may differ from SLUS_214.42. |
| Engine systems (physics, UI, inputs, audio parsers) | **Unchanged** — engine implementation, not tied to ELF addresses. |
| Character selection/combat/save/AI | **Unchanged** — planned but unimplemented regardless of region. |

### Warnings from existing docs
- `plan.md` line 593: "verify function addresses, asset names and binary layouts against the exact target release"
- `BUILD_STATUS.md` line 30: "verify regional/revision differences before treating any data offsets or runtime behavior as universal"

## 7. Next Steps & Open Questions

### Immediate next steps
1. **Obtain the SLES_541.61 ELF binary** and re-run disassembly to update all ELF address references currently tied to SLUS_214.42.
2. **Acquire actual game assets** (INFO.DAT, GAME.DAT, .stg, .hit, .mdl, .pmt) to validate format parsers against real data — currently all synthetic/test fixtures only.
3. **Build and compile SDL3 runtime** — currently blocked (SDL3 not installed, network blocked for FetchContent); CMake presets set `SDBZ_BUILD_GAME=OFF`.
4. **Run test suite** — CTest targets exist (`sdbz_tests`, player/controller/stage collision/spawn/animation/camera tests) but haven't been verified in this environment.

### Open questions
- What are the exact ELF addresses for SLES_541.61? (Cannot be derived without the binary.)
- Are STG/HIT node kinds 4/5/6 payloads documented? They are marked "opaque" in the parser.
- What is the full list of 25 SPT script names and their purposes?
- How does the EU beta INFO.DAT record layout differ from SLUS_214.42?
- Is there a `player_spawns.cfg` format beyond the empty template currently present?
- What animation clip names exist for each character, and what state transitions do they imply?

### Negative findings summary
| Area | Finding |
|------|---------|
| ELF binary files | None present |
| `readelf`/`objdump`/`strings` | Not available in environment |
| Actual game assets | None included (`assets/` empty) |
| `AGENTS.md` | Does not exist in this worktree |
| Combat system | Not implemented |
| Character selection | Not implemented |
| Move lists / frame data | Not present |
| Audio playback | Stub only (`play_buffer()` no-op) |
| Save data system | Not present |
| SDL3 runtime compilation | Not verified (SDL3 missing, network blocked) |
| Full stage/HIT corpus validation | Not performed |

No file changes were made during this research; all findings are read-only observations.