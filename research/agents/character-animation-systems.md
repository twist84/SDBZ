# Super Dragon Ball Z — Character & Animation Systems Research Report

Scope: European beta SLES_541.61 archive research, native C++ rebuild (SLUS_214.42 ELF reference),
and the local worktree at `.kilo/worktrees/character-systems`.

---

## 1. FMDL model format

### 1.1 DHIE hierarchy

`source/core/model.cpp` `parse_hierarchy`

- DHIE is a directory chunk with a normal 0x20-byte header and u32 child offsets.
- XHIE is a child of DHIE: 0x20-byte header followed by 0x40-byte node records.
- XNON holds the corresponding sequential NUL-terminated CP932 node names after its 0x20-byte header.

XHIE node record layout:

| Record offset | Type | Meaning |
|---|---|---|
| 0x00 | i32 | Node index |
| 0x04 | i32 | Parent index, -1 for a root |
| 0x08 | i32 | First child index, -1 for none |
| 0x0C | i32 | Next sibling index, -1 for none |
| 0x10 | float4 | Local scale XYZ, padding W |
| 0x20 | float4 | Local Euler angles XYZ in degrees, padding W |
| 0x30 | float4 | Local translation XYZ, padding W |

- Local transforms use `T * Rz * Ry * Rx * S` in column-vector notation.
- World transforms compose `parent * local`.
- The quaternion half-angle formula is confirmed in the original ELF at EE `0x1A0244–0x1A0300`, within the node creation routine at `0x1A0100`. That code converts degrees to radians and calculates quaternion XYZW; it is not an Euler-order guess based only on appearance.
- XMTX contains a 0x20-byte header followed by 32 i32 hierarchy indices per mesh (0x80 bytes). -1 denotes an unused slot. A rigid mesh attaches to slot zero's hierarchy node. Skinned vertex joint addresses select this palette. XMON contains sequential NUL-terminated mesh names.

### 1.2 XMOD geometry

`source/core/model.cpp` `parse_xmod`

- DMOD is a directory of XMOD chunks. The first DMA tag in each XMOD is at +0x1A0.
- XMOD +0x30 stores the vertex count, +0x34 the triangle count. +0x0C holds vertex flags, copied at +0x20. Other prefix fields are preserved/described conservatively in `arika_models.h`.
- Each DMA RET tag is 16 bytes, with a QWC in its low 16 bits. Its following QWC * 16 bytes form a VIF stream. Additional material sections follow as additional RET blocks in the same XMOD.
- All observed streams use NOP, STCYCL, UNPACK V4-32, and MSCNT. Each UNPACK has two float4-sized control/GIF vectors, then the vertex records. Its vertex count is the GIF tag's low 15 bits. This count and the flag-derived record stride exactly account for every UNPACK payload.
- Position XYZ and normal XYZ are floats. Position W is read as a u32: bit 15 suppresses a triangle at that vertex (ADC). A non-suppressed vertex forms a triangle with the two preceding vertices, including when they lie in the preceding UNPACK packet. Resetting strip history for each packet loses triangles. Normal W carries the authored orientation sign; swap the first two indices when it is negative. The exporter writes explicit triangle lists.
- STQ stores texture coordinates; the samples use the first two components as UV.
- The vertex-color vector contains PS2-scale RGBA floats (128 represents full scale). The exporter converts these to [0,1].
- Skin palette addresses are u32 values divisible by four; divide by four and index the mesh's XMTX row to obtain a hierarchy node. The following float4 supplies corresponding weights. Weights sum to one within float precision. Skinned positions are already in the authored model bind space. glTF inverse bind matrices are inverse hierarchy world matrices; rigid positions remain local to their attached node.

Vertex flag → stride table (fully decoded and enforced; parser rejects unknown flags):

| Flags | Vertex stride | Layout in 16-byte vectors |
|---|---|---|
| 0 / 1 | 48 | position, normal, STQ |
| 2 / 3 | 80 | position, normal, STQ, palette addresses, weights |
| 4 | 64 | position, normal, color, STQ |
| 6 | 96 | position, normal, color, STQ, palette addresses, weights |
| 11 | 80 | position, normal, STQ, second position, second normal |

- Flags 11 contain alternate position/normal data rather than bone addresses/weights. The exporter retains these and exposes the difference as a glTF morph target, initially at zero weight. Many are copies of the base data; others differ.
- One prop (`stg/sgmk01/sgmk01.mdl`) contains 20 vertices with negative weights / compensating weights above one. JSON preserves these. glTF requires nonnegative weights, so the exporter rejects that asset unless `--clamp-skin-weights` is supplied.
- Validation: 476 FMDL payloads parsed successfully, covering 14,800 mesh records, 4,824,347 stored vertices, and 2,425,131 triangles. The parser matches each XMOD's stored vertex and triangle counts, every DMA material assignment, and every VIF packet's record extent.
- Synthetic C++ test `test_fmdl_geometry` covers hierarchy node count/name, rigid mesh hierarchy attachment, vertex decoding, triangle-strip winding and indices, and OBJ export.

### 1.3 Materials and textures

`source/core/model.cpp` `parse_materials`

- DMAT is a directory containing XMAT and XMAL. XMAT has a 0x20-byte header and 0x30-byte records: two float4 color vectors followed by an i32 texture index and 12 further bytes. The complete shading meaning of the color vectors and remaining fields is not established; JSON calls them color_a and color_b.
- XMAL contains 32 i32 material indices per mesh, with -1 in unused slots. Each nonnegative index corresponds in order to one DMA block of that mesh. This correspondence holds across every model. The XMAT texture index selects an XPIX texture in the sibling FPIX file.
- Textures are decoded by the existing TIM2 exporter and embedded in glTF. Native outlines, cel shading, material animation, additive effects and precise transparency modes are not reproduced. Materials use double-sided alpha masking for inspection.
- 732 FPIX containers and 476 FMDL containers were found by signature. FPIX contains 3,438 XPIX chunks; each has a 32-byte wrapper followed by TIM2. Of those textures, 3,431 are indexed8, six indexed4, and one RGBA32. All have TIM2 version 4, format 1, one picture and one mipmap. Indexed palettes contain RGBA32 entries. Indexed8 CSM1 palette addressing swaps index bits 3 and 4.

---

## 2. MOT2 animation format

`source/core/animation.cpp` `parse_mot2` / `sample_animation_track`

### 2.1 Header

All offsets are relative to the MOT2 start. Integers are little-endian.

| Header offset | Type | Meaning |
|---|---|---|
| 0x00 | char[4] | MOT2 |
| 0x04 | u32 | Total size |
| 0x08 | float | Frames per second, 60 in supplied motions |
| 0x0C | u16 | Frame count |
| 0x0E | u16 | Motion-node count |
| 0x10 | u16 | Constant float4 count |
| 0x12 | u16 | Total animated channel count |
| 0x14 | u32 | Node descriptor array offset |
| 0x18 | u32 | Constant float4 pool offset |
| 0x1C | channel[1] | Translation channel group |
| 0x28 | channel[1] | Rotation channel group |
| 0x34 | channel[1] | Scale channel group |

### 2.2 Channel groups

Each 12-byte channel group contains `u16 channel_count, u16 key_count, u32 vectors_offset, u32 times_offset`. All channels in a group share a float time array, measured in seconds. Values are float4 and stored channel-major: all keys of channel zero, then all keys of channel one, etc. When channel_count is zero, key_count/time offsets may contain unused values and must not be dereferenced.

### 2.3 Node descriptors

Each 8-byte node descriptor contains `u16 flags, translation_index, rotation_index, scale_index`. Flag bits 1/2/4 select animated translation/rotation/scale. For an animated component, its index is a vector index into that group's array (`channel_number * key_count`). For a constant component, its index selects a float4 in the shared constant pool. Translation/scale use XYZ; rotations are XYZW quaternions. The constant pool ends exactly where the node descriptor table begins in every sample.

### 2.4 Interpolation / sampling

The loader at EE `0x1B12F0` relocates the eight offsets at `+0x14/+0x18/+0x20/+0x24/+0x2C/+0x30/+0x38/+0x3C`. It computes duration as `frame_count / fps`. The last authored frame is normally at `(frame_count - 1) / fps`. Component sampling reads the descriptor flags and indices, choosing a constant float4 or adjacent animated float4 values.
- Translation uses linear interpolation.
- The quaternion interpolation routine at EE `0x1B2C80` performs shortest-path slerp with a near-parallel linear fallback.
- The glTF exporter uses LINEAR rotation samplers, normalizes values, and fixes successive quaternion signs. It holds shorter/constant channels through the last authored frame.
- Looping behavior belongs to the game's caller and is not encoded as a glTF loop setting.
- No animated scale channels occur in the 1,399-motion sample, so dynamic-scale support follows the same header structure but has not been exercised on real assets.

### 2.5 Validation

All 57 RFSM packages / 1,399 MOT2 motions passed channel extent, time order, descriptor, and forward/reverse bone-binding checks. They contain 64,364 motion-node records, 49,759 constant vectors, 2,843 animated translation channels and 33,743 animated rotation channels.
- Synthetic C++ test `test_animation_formats` covers MOT2 clip name, node count, channel counts, animated translation track, constant rotation track, linear translation sampling, quaternion slerp (shortest path), RFSM shared bone names, RFSM motion count/name, and RFSM reverse bone binding.

---

## 3. RFSM / L3 motion packages

`source/core/animation.cpp` `parse_rfsm`

L3 is a tag here, not evidence of compression.

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | char[4] | RFSM |
| 0x04 | char[2] | L3 |
| 0x06 | u16 | Motion count |
| 0x08 | u32 | Shared block offset, 16 + count * 32 |
| 0x0C | u32 | Shared block end |

Each 32-byte directory record contains a 16-byte CP932 name, absolute motion offset, motion size, an unknown u32 (zero in sample), and a format value (0x10006 in sample).

The shared block begins with u16 motion count, u16 bone count, then three u32 absolute offsets: bone names, variable binding data, and bone flags. Bone names occupy fixed 16-byte records. There is one flag byte per bone; these are **not established parent indices**. The first MOT2 starts after the shared block, aligned to 16 bytes. MOT2 begins with its magic and total size.

### 3.1 Bone bindings

The variable area between bone names and bone flags begins with 12 bytes per motion:

| Record offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | Unknown/runtime pointer field, zero on disk in sample |
| 0x04 | u32 | Absolute channel-to-bone u16 array offset |
| 0x08 | u32 | Absolute bone-to-node i16 array offset |

- The forward array has one entry per animated channel: translation channels, then rotation channels, then scale channels. Each entry selects a shared RFSM bone name.
- The reverse array has one i16 per shared bone name: a MOT2 node index, or -1 if that bone is absent from this motion.
- Every animated channel's forward entry maps back to its node through the reverse table; this was validated for all 1,399 motions. Reverse mappings also name some nodes whose components are entirely constant.
- The glTF exporter matches these names to FMDL hierarchy names, preserving the model's rest transforms for unnamed/unmapped helper nodes. Names not found in the model are listed in animation extras.
- Raw standalone MOT2 files retain node indices but do not contain these shared names or the model hierarchy.

---

## 4. FMDL hierarchy ↔ MOT2 animation binding resolution

This section covers how forward channel→bone and reverse bone→node mappings are resolved, both at parse time (RFSM) and at runtime (viewer rendering).

### 4.1 Parse-time resolution (`parse_rfsm`, animation.cpp)

1. The RFSM shared block provides bone names (fixed 16-byte records) and a per-bone flag byte.
2. Each motion's 12-byte binding record gives the absolute offsets of the forward (channel→bone) and reverse (bone→node) arrays.
3. The forward array (one u16 per animated channel, ordered translation→rotation→scale) maps each animated channel to a shared RFSM bone index.
4. The reverse array (one i16 per shared bone) maps each shared bone to a MOT2 node index or -1.
5. `parse_rfsm` validates forward/reverse agreement: for every animated channel, `reverse[forward[channel]]` must equal the MOT2 node index that owns that channel. Mismatches throw.
6. MOT2 node descriptors (flags + translation/rotation/scale indices) reference animated channel vectors or constant pool entries. After binding, each MOT2 node that is targeted by a reverse binding gets its `m_name` set to the corresponding RFSM bone name. Conflicting names on one node throw.

Result: each MOT2 node either has a bone name (from RFSM reverse binding) or is unnamed (helper node). Animated channels are reachable through `node.m_name` → bone name.

### 4.2 Runtime resolution (`render_model_viewer`, model_renderer.cpp)

The viewer does **not** use the RFSM forward/reverse index arrays directly at render time. Instead it re-derives the mapping by string name:

1. Build `name_to_node`: `unordered_map<string, size_t>` from FMDL node name → node index.
2. For each animated clip node `an` (whose `m_name` came from an RFSM bone name):
   - `it = name_to_node.find(an.m_name)`. If not found, skip (continue).
   - Sample translation/rotation/scale tracks at `ctx.animation_time` via `sample_animation_track`.
   - Build local matrix `T * R * S` (quaternion → rotation matrix) and store in `local_xform[node_index]`, overriding the bind-pose local transform.
3. Compose world transforms: `world = build_world(local_xform, nodes)` = parent * local for each node.
4. Compute `inverse_bind[joint] = mat4_inverse(bind_world[joint])` from the bind-pose world transforms for skinning.

Key observation: the runtime binding is **name-based**, not index-based. The RFSM forward/reverse tables are fully validated at parse time but are not consulted during rendering; the renderer matches `s_animation_node.m_name` (RFSM bone name) to `s_model_node.m_name` (FMDL hierarchy name) by string lookup. This means:
- Any MOT2 node whose name doesn't exist in the FMDL hierarchy is silently skipped at render time.
- Bone names not found in the model are listed in animation extras (per model_format.md).
- There is no runtime validation that every animated channel's forward→reverse mapping still holds; that is a parse-time invariant only.

### 4.3 Semantic gap

The RFSM forward/reverse binding tables establish the authoritative channel→bone→node relationship at asset-authoring time, but the native renderer resolves animation at runtime purely by name matching between MOT2 node names and FMDL hierarchy node names. If the two naming schemes diverge (e.g. a bone renamed in the model but not in the motion package), the animation will not apply to that node and no error is raised at render time.

---

## 5. Skinning implementation

### 5.1 XMTX palette → hierarchy nodes

`source/core/model.cpp` `parse_fmdl` / `parse_xmod`

- XMTX holds 32 i32 hierarchy indices per mesh (0x80 bytes per palette row); -1 = unused slot.
- Each XMOD mesh gets one palette row (`palettes[palette_index][slot]`).
- XMOD skin palette addresses are u32 values divisible by 4. `joint = palette[raw_address / 4]`. The result is validated to be a valid hierarchy node index (`0 ≤ joint < node_count`, `joint ≤ 0xFFFF`).
- A rigid mesh (`m_is_skinned == false`) attaches to `palette[0]` via `m_rigid_node_index`.

### 5.2 Vertex skin data

`s_model_vertex` stores:
- `m_joints[4]` (std::uint16_t) — hierarchy node indices per influence.
- `m_weights[4]` (float) — per-influence weights.
- `m_is_skinned` (bool) — set true when skin data is parsed.

Constraints enforced in `parse_xmod`:
- `raw_address & 3 == 0` (address divisible by 4).
- `raw_address / 4 < palette.size()`.
- Weight sum must equal 1.0 within 1e-4, else parse fails.
- Zero-weight influences keep `joint = 0` but contribute nothing.

### 5.3 Skinning math (`skin_position`, math_helpers.cpp)

For each vertex `v` with position `pos`:

```
out = (0,0,0); total_weight = 0
for i in 0..3:
    w = v.m_weights[i]
    if w <= 0: continue
    joint = v.m_joints[i]
    if joint >= nodes.size(): continue
    bone_local = inverse_bind[joint] * pos        // object → bind space of joint
    skinned = world[joint] * bone_local           // bind space → world space of joint
    out += w * skinned
    total_weight += w
if total_weight > 0:
    out /= total_weight
else:
    // rigid fallback: use palette slot 0 (rigid_node) world transform on raw pos
    out = world[rigid_node] * pos
```

- `inverse_bind[joint]` = inverse of bind-pose world matrix for that joint (computed per-frame in the viewer from `bind_local → build_world → mat4_inverse`).
- `world[joint]` = current animated world matrix for that joint.
- Skinned positions are stored in authored model bind space, so the inverse-bind → world sandwich is correct.
- The viewer normalizes by `total_weight`; the original game may not (weights already sum to 1).

### 5.4 Rigid meshes

- `mesh.m_is_skinned == false`, `mesh.m_rigid_node_index = palette[0]`.
- `skin_position` falls back to `world[rigid_node] * pos` when no influences have weight > 0.
- In `draw_primitive`, rigid nodes use `mesh.m_rigid_node_index`; skinned vertices use the full 4-influence blend.

---

## 6. Morph targets (flag 11)

### 6.1 Format

`source/core/model.cpp` `parse_xmod`

- Flags bit 8 (`0x08`) = morph. Flag value 11 = base(1) + skinned(2) + morph(8).
- Stride 80: position, normal, STQ, second position, second normal.
- Morph vectors are `vectors[vectors.size()-2]` (morph position) and `vectors[vectors.size()-1]` (morph normal).
- Stored in `s_model_vertex.m_morph_position` / `m_morph_normal`, flag `m_has_morph`.
- `s_model_mesh.m_has_morph` set from `morph` bool.
- Many morph targets are copies of the base data; others differ (e.g. facial expression shapes).
- The facial controller (per FACE_PLAYBACK.md / ACTION_CONTROL.md) can drive these morphs and texture endpoints in baked glTF animations.

### 6.2 Current handling in the native codebase

- Parsed and stored in `s_model_vertex` / `s_model_mesh` (fields exist).
- Exported to glTF as a morph target at zero weight (`--clamp-skin-weights` for negative weights).
- **NOT applied at render time.** `model_renderer.cpp` `draw_primitive` / `skin_position` only perform skeletal skinning; morph targets are ignored. There is no morph-weight parameter, no morph target selection, and no facial animation playback in the C++ viewer.
- `part_groups` (part_groups.cpp/hpp) provides per-part mesh visibility cycling (hair, hands, face parts) but that is alternate-mesh selection, not morph interpolation.

---

## 7. PMT clip names → animation states

`source/gameplay/player_animation.hpp` / `player_animation.cpp`, driven in `source/platform/sdl_main.cpp`

### 7.1 State enum

```cpp
enum class e_player_animation_state
{
    idle,
    walk,
    run,
    jump,
    fall,
    land
};
```

### 7.2 State classification (`classify_player_animation_state`)

1. If `landing_override` is true → `land`.
2. If `!controller.m_grounded`:
   - `controller.m_velocity[1] > 0.15f` → `jump`
   - otherwise → `fall`
3. If grounded:
   - Horizontal speed squared = `velocity.x^2 + velocity.z^2`
   - If `≤ 0.04f` → `idle`
   - Else `controller.m_running` ? `run` : `walk`

### 7.3 Clip name matching (`find_player_animation_clip`)

Case-insensitive substring token match per state:

| State | Tokens | Fallback |
|---|---|---|
| idle | idle, wait, stand, stnd | — |
| walk | walk, move, step | idle |
| run | run, dash, sprint | walk → idle |
| jump | jump, jamp, takeoff | — |
| fall | fall, airborne, air | jump |
| land | land, touchdown, touch | idle |

- Exact match tried first; then fallbacks in order. Returns `std::nullopt` when no match — deliberately does not guess a numeric clip index.
- Synthetic C++ test `test_player_animation.cpp` covers idle/walk/run/jump/fall/land classification and clip selection including fall→jump fallback and unmatched-land nullopt.

### 7.4 Runtime selection (`sdl_main.cpp`)

- PMT clip selection is name-driven rather than index-driven. If a package has no recognizable state names, the runtime keeps its current preview clip instead of guessing an animation index.
- `player_controller.m_landed_this_frame` arms the `land` clip for `max(clip.m_duration, 0.1f)`.
- `landing_override` set while `player_land_animation_remaining > 0.0f`, decremented each frame.
- When `selected_clip` differs from current `motion_clip_index` or state changed, `motion_clip_index` is updated and `animation_time` reset to 0.
- Animation time advances by frame delta and loops at `clip.m_duration`.

---

## 8. Player / fighter runtime (implemented)

`source/gameplay/player_controller.cpp` / `player_controller.hpp`, `player_spawn.cpp` / `player_spawn.hpp`

- Fixed 60 Hz stepped movement with bounded accumulator; rendering independent of simulation.
- Acceleration/deceleration, walk/run speed, jump/gravity, camera-relative input (negative-orbit side adds 180°).
- World-space transformed HIT collision: ground support, capsule clearance, wall blocking/sliding, overhead clearance, edge falls.
- Stage-aware spawn with `player_spawns.cfg` per-stage overrides; fallback chooses a large walkable HIT polygon with clear capsule footprint.
- Default `ply/p01/p01a.mdl` auto-load on stage completion through shared archive model loader (including `.pix` and `.pmt` companions).
- Stage transitions invalidate previous player session before resources cleared/parsed.
- Five CTest targets pass under GCC 14.2 (Release + ASan/UBSan).

---

## 9. Character selection (Phase 7) — data structures needed

Character selection is **planned but unimplemented**. `plan.md` Phase 7 (Menus and game modes) lists it as milestone M9. The suggested source layout includes `source/ui/character_select.cpp`, but no such file exists. The debug menu exposes only Stage and Motion clip cycling; there is no character roster, select screen, or per-character model assignment.

To implement character selection, the following data structures would be needed:

### 9.1 Character roster

```cpp
struct s_character_def
{
    std::string m_id;           // e.g. "goku", "vegeta"
    std::string m_name;         // display name
    std::string m_model_path;   // archive path to .mdl
    std::string m_texture_path; // archive path to .pix (or auto-resolved)
    std::vector<std::string> m_motion_paths; // .pmt packages
    std::vector<std::size_t> m_part_groups; // alternate parts (hair/hands/face)
    // unlock state, cost, etc.
};

struct s_character_roster
{
    std::vector<s_character_def> m_characters;
    std::size_t m_selected_p1 = 0;
    std::size_t m_selected_p2 = 0;
};
```

### 9.2 Per-character model assignment

- Character → FMDL model + companion `.pix` texture + `.pmt` motion packages (already supported by VFS companion-asset loader for a single model).
- Per-character part groups: alternate meshes (hair, hands, face) mapped via `part_base_name` grouping.
- Per-character default animation clip (idle) and motion package.

### 9.3 Fighter object layout (research target)

The cited original-game offsets are **not reverse-engineered or implemented here**:
- `+0x510` spawn vec4 — not present
- `+0x520` flags — not present
- `+0x534` side selector (P1/P2) — not present
- `+0x538/+0x53c` vtable index — not present

No `fighter` struct exists in the codebase (grep for these offsets and `fighter` found nothing across source and tests). The current `s_player_controller` is a fresh implementation (position, velocity, yaw, speeds, gravity, grounded/running/landed flags), not a port of the original fighter object layout.

To reconstruct the original fighter object, ELF analysis would be needed to map:
- Spawn position vector (+0x510)
- Character/state flags (+0x520)
- Side selector P1/P2 (+0x534)
- Vtable / function-pointer indices (+0x538, +0x53c)
- Health, ki, stamina meters
- Current move / frame data pointer
- Animation state and clip index

### 9.4 Additional systems needed beyond the roster

- Character select screen UI (grid of character portraits, P1/P2 cursors, confirm/back).
- Per-character asset preloading (model, texture, motions).
- Side selector (P1 on left, P2 on right, mirrored scaling).
- Fighter state machine (idle, walk, jump, attack, hitstun, block, etc.).
- Move lists / frame data (absent — no startup/active/recovery, damage, cancel windows).
- Health/ki/stamina resources.
- Round manager and victory conditions.

---

## 10. What's implemented vs planned

### Implemented (decoded and tested)
- FMDL geometry: DHIE/XHIE/XNON hierarchy, XMTX palettes, XMOD vertex flags/strides/DMA/VIF parsing, skinning, morph targets (flags 11), ADC triangle suppression, orientation sign, materials/textures (XMAT/XMAL/FPIX/TIM2).
- MOT2 animation: header, channel groups, node descriptors, linear translation interpolation, shortest-path quaternion slerp with linear fallback, constant tracks, duration/last-frame computation.
- RFSM/L3 packages: directory records, shared block (bone names/variable bindings/bone flags), forward channel→bone and reverse bone→node bindings validated for all 1,399 motions.
- FMDL↔MOT2 binding: parse-time forward/reverse validation; runtime name-based resolution in viewer (semantic gap noted).
- Skinning: XMTX palette→hierarchy node mapping, 4-influence vertex skinning, inverse-bind→world sandwich, rigid fallback, weight normalization.
- Morph targets: parsed and stored; exported as glTF morph targets at zero weight; **not applied at render time**.
- PMT clip → animation state mapping (idle/walk/run/jump/fall/land) with name-based matching and fallbacks.
- Player controller: fixed 60 Hz movement, camera-relative input, jump/gravity, transformed HIT collision, spawn resolution, `player_spawns.cfg`, default player model auto-load.
- **SLES_541.61 ELF analysis**: format strings (%s.mdl/%s.pix/%s.pmt/MOT2/FMDL), class names (CEBZ*, CMotionData/Handle, CSceneFrame, CClipNode, CFaceAni*, CMenu*, CPlayer*, CPl*), character selection strings ("Character Select", "Select Character", "Player Character"), bone names (HipBone, TailA-D, ClothBoneA/B), verified -0x24C0 SLUS→SLES address shift, animation playback code locations (MOT2 loader ~0x001AEDE0, slerp ~0x001B0770, string hash ~0x0018F9E8, node creation ~0x0019DBF0), FPU-dense regions for animation math (0x004BB000, 0x004BA000, 0x00186000), player/fighter classes (CPlayerBase at 0x004C0B10, CPlayerMotion at 0x004C1098, CPlayerMgr at 0x004C0B88, CPlayerFlag at 0x004C1928, CPl01-CPl30).
- Build/tests: GCC 14.2 Debug/Release and Clang 17 Release pass; ASan/UBSan pass with leak detection. Five CTest targets. SDL3 runtime not compiled in this environment (SDL3 not installed).

### Planned / unimplemented / absent
- Character selection (planned Phase 7 / milestone M9; SLES strings confirm the system exists in the original game but no native implementation).
- Move lists / frame data (absent).
- Fighter object field layout offsets (+0x510/+0x520/+0x534/+0x538/+0x53c) — **not found as direct immediates in SLES_541.61 ELF**; requires MIPS disassembly to reconstruct. No `fighter` struct exists in the codebase.
- **Animation playback code in ELF** — MOT2 loader, slerp, string hash, state machine not verified at predicted SLES addresses; requires direct MIPS disassembly. Animation state names (idle/walk/run/jump/fall/land) exist only in asset files, not the ELF.
- Blend/crossfade code — not identified in SLES ELF; FPU-dense regions exist but not mapped to blend functions.
- Morph target application at render time (parsed/stored but not used).
- SDL3 game runtime (target exists, not compiled/verified here).
- Real INFO.DAT/GAME.DAT asset corpus (not present; parsers validated against synthetic fixtures and the checked-in European-beta INFO index fixture).

---

## 11. SLES_541.61 ELF analysis

### 11.1 Address shift verification

The address shift between SLUS_214.42 and SLES_541.61 was verified by comparing format string locations:

| Format string | SLUS vaddr | SLES vaddr | Shift |
|---|---|---|---|
| `%s.mdl` | 0x004CC5A8 | 0x004CA0E8 | -0x24C0 |
| `%s.pix` | 0x004CC5B0 | 0x004CA0F0 | -0x24C0 |
| `%s.pmt` | 0x004CC728 | 0x004CA268 | -0x24C0 |
| `CEBZData` class name | 0x004CEF18 | 0x004CCA48 | -0x24C0 |

**Verified shift: -0x24C0** (not -0x2510 as previously assumed). File offset → vaddr: `vaddr = file_offset + 0x000FFF80`.

### 11.2 Key string locations in SLES_541.61

`source` analysis of `C:\Dev\SuperDragonBallZ\assets\original\SLES_541.61` (4,180,784 bytes, 32-bit MIPS ELF, ET_EXEC, entry 0x00100008). Main LOAD segment: file off 0x80 → vaddr 0x00100000, size 0x3FC900.

#### Format strings

| String | File offset | SLES vaddr | SLUS vaddr (ref) |
|---|---|---|---|
| `%s.mdl` | 0x003CA168 | 0x004CA0E8 | 0x004CC5A8 |
| `%s.pix` | 0x003CA170 | 0x004CA0F0 | 0x004CC5B0 |
| `%s.pmt` | 0x003CA2E8 | 0x004CA268 | 0x004CC728 |
| `%s.ani` | not found directly | ~0x004CD630 (shifted) | 0x004CFAF0 |
| `%s.pas` | not found directly | ~0x004CD608 (shifted) | 0x004CFAC8 |
| `%s.cht` | not found directly | ~0x004C81F8 (shifted) | 0x004CA6B8 |
| `%s.ebz` | not found directly | ~0x004CCA68 (shifted) | 0x004CEF28 |
| `MOT2` magic | 0x003C05A0 | 0x004C0520 | — |
| `FMDL` magic | 0x003B4D90 | 0x004B4D10 | — |

#### Class names (RTTI/debug strings)

| Class | File offset | SLES vaddr |
|---|---|---|
| `CModelHandle` | 0x003CA178 | 0x004CA0F8 |
| `CTextureHandle` | 0x003CA1A0 | 0x004CA120 |
| `CMotionData` | 0x003CA2C0 | 0x004CA240 |
| `CMotionHandle` | 0x003CA2F0 | 0x004CA270 |
| `CSceneFrame` | 0x003C0960 | 0x004C08E0 |
| `CClipNode` | 0x003C09E8 | 0x004C0968 |
| `CEBZData` | 0x003CCAC8 | 0x004CCA48 |
| `CEBZHandle` | 0x003CCAE0 | 0x004CCA60 |
| `CEBZBase` | 0x003CCAF0 | 0x004CCA70 |
| `CEBZEffectObj` | 0x003CCB00 | 0x004CCA80 |
| `CEBZEffMatrix` | 0x003CCBD0 | 0x004CCB50 |
| `CEBZEffectEle` | 0x003CCBE0 | 0x004CCB60 |
| `CEBZEffectGen` | 0x003CCBF0 | 0x004CCB70 |
| `CEBZEffectBase` | 0x003CCC00 | 0x004CCB80 |
| `CFaceAniData` | 0x003CD690 | 0x004CD610 |
| `CFaceAniHandle` | 0x003CD6A8 | 0x004CD628 |
| `CMenuMainMenu` | 0x003CC980 | 0x004CC900 |
| `CMenuFuncMng` | 0x003CB9E0 | 0x004CB960 |
| `CMenuSelBase` | 0x003CBAE0 | 0x004CBA60 |

#### Character class names

`CPl11` (0x003CB860), `CPl26` (0x003CC208), `CPl27` (0x003CC6D8), `CPl28` (0x003CC350), `CPl29` (0x003CC3E0), `CPl30` (0x003CC4E0) — player character class names found in the binary.

#### Bone names (animation hierarchy)

`HipBone` (0x003C0C70), `TailABone` (0x003C0CE8), `TailBBone` (0x003C0CF8), `TailCBone` (0x003C0D08), `TailDBone` (0x003C0D18), `ClothBoneA` (0x003CC5C0), `ClothBoneB` (0x003CC5D0).

### 11.3 Character selection code paths (SLES)

| String | File offset | SLES vaddr | Context |
|---|---|---|---|
| "Character Select" | 0x003D36D0 | 0x004D3650 | Main character select screen |
| "Select Character" | 0x003D2D80 | 0x004D2D00 | Character selection prompt |
| "Finish selecting characters" | 0x003D2A50 | 0x004D29D0 | Return to main menu confirm |
| "Player Character" | 0x003D3E00 | 0x004D3D80 | P1 character label |
| "Character Change" | 0x003D3EB0 | 0x004D3E30 | Character change option |
| "Character Customization" | 0x003D2BA0 | 0x004D2B20 | Customize mode |
| "Create Character Card" | 0x003D2BC0 | 0x004D2B40 | Card creation |
| "Versus" | 0x003D3BA8 | 0x004D3B28 | Versus mode |
| "Battle Stage Setting" | 0x003D2AD0 | 0x004D2A50 | Stage select |

Character card system: 30 character card limit, memory card save/load, color selection, inherited skill selection.

### 11.4 Animation-related code strings

| String | File offset | SLES vaddr | Context |
|---|---|---|---|
| `MOT2` magic | 0x003C05A0 | 0x004C0520 | MOT2 format identifier |
| `frame  %4.2ffps  Node=%d` | 0x003C05C9 | 0x004C0549 | Debug animation output |
| `MotList%03d` | 0x003CA2D0 | 0x004CA250 | Motion list naming pattern |
| `invalid motion_type(` | 0x003D9458/78/98 | ~0x004D6F38 | Motion type validation errors |

No standalone `idle`/`walk`/`run`/`jump`/`fall`/`land` state strings found in the ELF — animation state names are stored in asset files (PMT/RFSM), not the executable. The strings `jump` (0x003CF402), `fall` (0x003CE2FE), `land` (0x003CF2B9) appear only as substrings of larger debug/descriptive text.

### 11.5 Fighter object layout offsets

The cited fighter object offsets (**+0x510 spawn vec4, +0x520 flags, +0x534 side selector, +0x538/+0x53c vtable index**) were **not found** as direct 16-bit or 32-bit immediates in the SLES_541.61 ELF .text section. A grep for `fighter` across the binary found only descriptive text ("The fighter gets an..." at 0x003CE930), not a fighter struct definition or accessor code.

This means:
- The fighter object layout must be reconstructed through deeper MIPS disassembly (tracking register usage, stack offsets, and struct member access patterns).
- The offsets may be computed dynamically (base + variable offset) rather than as fixed immediates.
- No `fighter` C++ struct exists in the native codebase; `s_player_controller` is a fresh implementation.

### 11.6 SLUS → SLES address mapping (using -0x24C0 shift)

| Item | SLUS vaddr | SLES vaddr (predicted) | Verified in SLES |
|---|---|---|---|
| CEBZ loading code | 0x003301E0–0x00330300 | 0x0032DD20–0x0032DE20 | Not verified |
| String hash function | 0x00191EF8 | 0x0018FA38 | Not verified |
| Data unpacker | 0x00191638 | 0x0018F178 | Not verified |
| CEBZ class info block | 0x004FE628 | 0x004FC168 | Not verified |
| CEffectMgr class name | 0x004CA870 | 0x004C83B0 | Not verified |
| "%s.ebz" format string | 0x004CEF28 | 0x004CCA68 | Not verified |

### 11.7 Player/fighter-related class names (SLES)

| Class | File offset | SLES vaddr | Context |
|---|---|---|---|
| `CPlayerBase` | 0x003C0B90 | 0x004C0B10 | Base player/fighter class |
| `CPlayerGage` | 0x003C0BA0 | 0x004C0B20 | Player gauge (health/ki) |
| `CPlayerHitTimer` | 0x003C0BB0 | 0x004C0B30 | Player hit timer |
| `CPlayerMgr` | 0x003C0C08 | 0x004C0B88 | Player manager |
| `CPlayerModel` | 0x003C10E8 | 0x004C1068 | Player model |
| `CPlayerMotion` | 0x003C1118 | 0x004C1098 | Player motion/animation |
| `CPlayerPad` | 0x003C1188 | 0x004C1108 | Player input pad |
| `CPlayerScript` | 0x003C11B8 | 0x004C1138 | Player script |
| `CPlayerShellMode` | 0x003C11F0 | 0x004C1170 | Player shell mode |
| `CPlayerSpWork` | 0x003C1208 | 0x004C1188 | Player special work |
| `CPlayerFlag` | 0x003C19A8 | 0x004C1928 | Player flags |
| `CPlayerCmdWork` | 0x003C17B0 | 0x004C1730 | Player command work |
| `CPlayerBodyInfo` | 0x003C1810 | 0x004C1790 | Player body info |
| `CPlayerExtSystem` | 0x003C18F0 | 0x004C1870 | Player extended system |
| `CPlayerSE` | 0x003C15F8 | 0x004C1578 | Player sound effects |
| `CPlayerVoiStrem` | 0x003C17A0 | 0x004C1720 | Player voice stream |
| `CPlayerEnhancer` | 0x003C1800 | 0x004C1780 | Player enhancer |
| `CPlSaiyaBase` | 0x003C9D10 | 0x004C9C90 | Saiya base character |

Character class names: `CPl01`–`CPl30` (20+ playable characters), `CPl09`–`CPl12`, `CPl22`–`CPl30`, `CPlAfterProc`, `CPlSaiyaBase`.

### 11.8 Logic comparison: SLES vs SLUS

Format strings and class names match between SLUS_214.42 and SLES_541.61 with a consistent -0x24C0 shift, indicating the two releases share the same codebase with different compilation/linking offsets. The CEBZ effect system classes (`CEBZData`, `CEBZEffectObj`, etc.), player classes (`CPlayerBase`, `CPlayerMotion`, `CPlayerMgr`, etc.), and menu classes (`CMenuMainMenu`, `CMenuSelBase`) are present in both. Character class names (`CPl11`–`CPl30`) indicate 20+ playable characters. No evidence of fighter object layout or animation state machine code was found at the expected shifted addresses — these require direct MIPS disassembly of the SLES binary.

---

## 12. SLES_541.61 ELF — Animation Playback Code

### 12.1 MOT2 motion sampling code

No direct MOT2 loader was found at the SLUS-predicted address (EE 0x001B12F0 → SLES ~0x001AEDE0 with -0x2510 shift). Code at the predicted location contains branching and stack setup but no clear MOT2 header parsing.

**Motion-related classes found:**
- `CMotionData` at SLES vaddr 0x004CA240 (file 0x003CA2C0) — motion data class
- `CMotionHandle` at SLES vaddr 0x004CA270 (file 0x003CA2F0) — motion handle class
- `CSceneFrame` at SLES vaddr 0x004C08E0 (file 0x003C0960) — scene frame/animation class
- `CClipNode` at SLES vaddr 0x004C0968 (file 0x003C09E8) — clip node class
- `MotList%03d` format string at SLES vaddr 0x004CA250 (file 0x003CA2D0) — motion list naming pattern
- `frame  %4.2ffps  Node=%d` at SLES vaddr 0x004C0549 (file 0x003C05C9) — debug animation output

**Model loading code references** (LUI to 0x4CA0 = `%s.mdl`/`%s.pix` upper):
- SLES vaddr 0x001BC480 — `lui a0, 0x4CA0` followed by store operations, likely model handle setup
- SLES vaddr 0x002FF100 — `lui a0, 0x4CA0`, then `lui`, 0x005E; `lui`, 0x004D; `jal 0x00190D50` — model loading function call
- SLES vaddr 0x002FF234 — `lui a0, 0x4CA0` followed by store operations
- JAL target 0x00190D50 — model loading function entry (stack setup, register saves)

### 12.2 Animation state machine

No animation state enum values (idle=0, walk=1, run=2, jump=3, fall=4, land=5) were found as immediates in MIPS comparison instructions. No state transition trigger code (idle→walk→run→jump→fall→land) was identified at shifted SLUS addresses.

Animation state names are **not** in the ELF — they exist only in asset files (PMT/RFSM). The strings `jump` (file 0x003CF402), `fall` (file 0x003CE2FE), `land` (file 0x003CF2B9) appear only as substrings of larger descriptive text, not as state identifiers.

`motion_type(%d)-0/1/2` debug strings found at file 0x003D9458/78/98 (SLES vaddr ~0x004D6F38) — indicates motion type indexing exists but state names are not in the binary.

### 12.3 Clip name hashing

SLUS string hash function at EE 0x00191EF8 → SLES predicted ~0x0018F9E8–0x0018FA38 (with -0x2510/-0x24C0 shift). Code at SLES 0x0018F9E8 shows stack setup and register saves consistent with a function prologue, but no JAL references to this address were found in the .text section — the hash may be inlined or called through a different mechanism.

No clip name hashing call sites were identified in the SLES binary. The native codebase uses case-insensitive substring token matching instead of hash-based lookup.

### 12.4 Blend/crossfade code

No animation blend or crossfade code was identified in the SLES ELF. FPU instruction density analysis shows regions with high FPU concentration:
- SLES vaddr 0x004BB000: 92 FPU instructions
- SLES vaddr 0x004BA000: 87 FPU instructions
- SLES vaddr 0x00186000: 87 FPU instructions
- SLES vaddr 0x00137000: 83 FPU instructions

These regions likely contain transformation/animation math (slerp, linear interpolation, matrix math) but were not specifically mapped to blend/crossfade functions.

### 12.5 SLUS → SLES predicted addresses for animation systems

| Item | SLUS vaddr | SLES predicted (-0x2510) | SLES predicted (-0x24C0) | Status |
|---|---|---|---|---|
| MOT2 loader | 0x001B12F0 | 0x001AEDE0 | 0x001AEE30 | Code present, not verified as MOT2 loader |
| Slerp (quaternion) | 0x001B2C80 | 0x001B0770 | 0x001B07C0 | Code present, FPU ops visible |
| Node creation | 0x001A0100 | 0x0019DBF0 | 0x0019DC40 | Code present, stack/reg save |
| String hash | 0x00191EF8 | 0x0018F9E8 | 0x0018FA38 | Function prologue visible |
| Data unpacker | 0x00191638 | 0x0018F128 | 0x0018F178 | Not verified |
| CEBZ loading | 0x003301E0–0x00330300 | 0x0032DCD0–0x0032CEDF | 0x0032DD20–0x0032DE20 | Not verified |

### 12.6 Logic comparison: SLES vs SLUS (animation)

- **MOT2 format**: Same magic and header structure confirmed in both releases (MOT2 string at SLUS unknown → SLES 0x004C0520).
- **Animation classes**: `CMotionData`, `CMotionHandle`, `CSceneFrame`, `CClipNode` present in SLES, matching the animation system architecture.
- **Debug output**: `frame %4.2ffps Node=%d` string present in SLES at 0x004C0549, confirming per-frame animation debugging exists.
- **State machine**: Not found in either release's ELF at the expected locations — animation state logic is likely in asset-driven code or requires deeper disassembly.
- **Slerp/linear interp**: FPU code present at predicted addresses but not specifically verified as slerp implementation.
- **Clip selection**: Name-based matching in native codebase; hash-based lookup in original ELF not confirmed.

---

## 13. SLES_541.61 ELF — Fighter Object Layout & State Machine

### 13.1 Fighter object field offsets — NOT FOUND

The cited fighter object offsets (**+0x510 spawn vec4, +0x520 flags, +0x534 side selector, +0x538/+0x53c vtable index**) were **not found** as direct 16-bit or 32-bit immediates in the SLES_541.61 ELF .text section.

| Offset | Meaning | Found in SLES ELF? |
|---|---|---|
| +0x510 | Spawn vec4 | No |
| +0x520 | Flags | 1 incidental ref at vaddr 0x0014538C (not verified as fighter field) |
| +0x534 | Side selector (P1/P2) | No |
| +0x538 | Vtable index | No |
| +0x53C | Vtable index | No |

The single reference to imm 0x0520 at SLES vaddr 0x0014538C was disassembled as `bltz $t1, ...` within a function — not a fighter field access. No `fighter` C++ struct exists in the native codebase; `s_player_controller` is a fresh implementation.

**Blockers:**
- Fighter object layout must be reconstructed through deeper MIPS disassembly (tracking register usage, stack offsets, and struct member access patterns).
- Offsets may be computed dynamically (base + variable offset) rather than as fixed immediates.
- The ELF contains class names (`CPlayerBase`, `CPlayerMotion`, `CPlayerFlag`, `CPlayerMgr`) but no fighter struct layout information.

### 13.2 Animation state transitions — NOT FOUND

No animation state machine code (idle→walk→run→jump→fall→land transitions) was found in the SLES ELF. No state enum values were found as immediates in comparison instructions. Animation state names exist only in asset files (PMT/RFSM clip names), not in the executable.

### 13.3 Fighter clip selection/playback — PARTIALLY FOUND

**Clip-related code strings:**
- `clip is not set.` at file 0x003BD623/653 (SLES vaddr ~0x004BD5A3/~0x004BD5D3)
- `MotList%03d` at SLES vaddr 0x004CA250 (file 0x003CA2D0) — motion list naming pattern
- `invalid motion_type(` at file 0x003D9458/78/98 (SLES vaddr ~0x004D6F38)
- `motion_type(%d)-0/1/2` at SLES vaddr ~0x004D6F38

**Motion-related classes:** `CMotionData` (0x004CA240), `CMotionHandle` (0x004CA270), `CPlayerMotion` (0x004C1098)

No direct clip selection/playback function was identified at shifted SLUS addresses.

### 13.4 Character selection menus — FOUND (strings only)

| String | File offset | SLES vaddr | Context |
|---|---|---|---|
| "Character Select" | 0x003D36D0 | 0x004D3650 | Main character select screen |
| "Select Character" | 0x003D2D80 | 0x004D2D00 | Character selection prompt |
| "Finish selecting characters" | 0x003D2A50 | 0x004D29D0 | Return to main menu confirm |
| "Player Character" | 0x003D3E00 | 0x004D3D80 | P1 character label |
| "Character Change" | 0x003D3EB0 | 0x004D3E30 | Character change option |
| "Character Customization" | 0x003D2BA0 | 0x004D2B20 | Customize mode |
| "Create Character Card" | 0x003D2BC0 | 0x004D2B40 | Card creation |
| "Versus" | 0x003D3BA8 | 0x004D3B28 | Versus mode |
| "Battle Stage Setting" | 0x003D2AD0 | 0x004D2A50 | Stage select |

Character card system: 30 character card limit, memory card save/load, color selection, inherited skill selection. No character selection menu code was found at shifted SLUS addresses — only the string resources exist in the binary.

### 13.5 Side selector field access — NOT FOUND

No side selector field access (+0x534→+0x08) was found in the SLES ELF. The string `side.` appears at file 0x003B5F5C (in debug context, not field access code). Side selector (P1/P2) logic requires deeper MIPS disassembly.

### 13.6 Vtable index field access — NOT FOUND

No vtable index field access (+0x538/+0x53c) was found in the SLES ELF. Vtable-related code requires deeper MIPS disassembly of class constructor/virtual method tables.

---

## 14. SLES_541.61 ELF — Summary of What Was Found vs Not Found

### Found in SLES ELF

| Item | SLES Address | Verified |
|---|---|---|
| Format strings (%s.mdl, %s.pix, %s.pmt) | 0x004CA0E8, 0x004CA0F0, 0x004CA268 | Yes |
| MOT2 / FMDL magic strings | 0x004C0520, 0x004B4D10 | Yes |
| Class names (CPlayer*, CMotion*, CEBZ*, CMenu*, CPl*) | Various | Yes |
| Character selection strings | 0x004D29D0–0x004D3E30 | Yes |
| Bone names (HipBone, TailA-D, ClothBoneA/B) | 0x004C0C70–0x004C0D18, 0x004CC5C0–0x004CC5D0 | Yes |
| Player classes (CPlayerBase, CPlayerMotion, CPlayerMgr, etc.) | 0x004C0B10–0x004C1928 | Yes |
| Motion list pattern (MotList%03d) | 0x004CA250 | Yes |
| Debug animation output (frame %4.2ffps Node=%d) | 0x004C0549 | Yes |
| FPU-dense regions (potential animation math) | 0x004BB000, 0x004BA000, 0x00186000, 0x00137000 | Yes |
| String hash function (prologue only) | ~0x0018F9E8 | Partial |
| Model loading code (JAL target 0x00190D50) | 0x00190D50 | Partial |

### Not found in SLES ELF

| Item | Reason |
|---|---|
| Fighter object layout (+0x510/+0x520/+0x534/+0x538/+0x53c) | Not as direct immediates; requires MIPS disassembly |
| Animation state machine (idle/walk/run/jump/fall/land) | State names only in assets, not ELF |
| Animation state enum values as immediates | Not found in MIPS comparisons |
| Blend/crossfade code | Not identified; FPU regions not mapped |
| Clip selection/playback functions | Not found at predicted addresses |
| Side selector field access (+0x534) | Not found |
| Vtable index field access (+0x538/+0x53c) | Not found |
| MOT2 loader at SLUS-predicted address | Code present but not verified |
| Slerp at SLUS-predicted address | Code present but not verified |
| String hash call sites | No JAL refs found |
| Character selection menu code | Strings found but no code at shifted addresses |

### Address shift

**Verified shift: -0x24C0** (SLUS → SLES), confirmed by matching format strings and class names. The previously assumed -0x2510 shift is incorrect.


