# Super Dragon Ball Z Recomp — Camera Class Investigation

## Scope
Search `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\` (recompiled game unit files) for a camera class, and compare with the native SDL3 runtime camera in `C:\Dev\SuperDragonBallZ\.kilo\worktrees\recomp-build-test\source\gameplay/camera.hpp`.

## Finding: No camera class exists in the generated recomp code

The `generated\SuperDragonBallZ\` directory contains only machine-code translations of game ELF functions (`unit_0000.cpp` through `unit_01F8.cpp`), each a flat `void unit_XXXX(Cpu &c, uint32_t entry, uint32_t depth)` function with a `switch(entry)` dispatch and labeled basic blocks. There are **no `struct`/`class` definitions, no `vtable` pointers, and no named object fields** anywhere in the generated code.

Specifically:
- `table.cpp` registers 240 compiled units by entry-point address; no class/vtable info.
- `units.hpp` declares only `unit_XXXX(Cpu &, uint32_t, uint32_t)` function prototypes and `entries_XXXX[]` jump tables.
- Every `unit_XXXX.cpp` contains raw register/memory operations (`c.r[N]`, `c.f[N]`, `ld/st` loads/stores, FP math) — no `this` pointer, no `struct Camera`, no `Camera::` methods, no vtable loads at `+0x00`.

Camera-related float constants (e.g. `0x3F800000` = `1.0f`, `0x3E800000` = `0.25f`) appear as immediate values across many unit files (e.g. `unit_0028.cpp:2319`, `unit_0021.cpp:691`, `unit_0000.cpp:3173`), but they are generic FP constants used in arbitrary math, not identifiable camera fields, and cannot be attributed to a camera struct without source-level type info.

## Native SDL3 runtime camera (reference implementation)

The separate native SDL3 runtime in this worktree defines an explicit camera struct:

`source/gameplay/camera.hpp:10-24` — `struct s_camera` (plain POD, **no vtable**, no virtual methods):

| Offset (approx.) | Field | Type | Default | Meaning |
|---|---|---|---|---|
| +0x00 | `m_yaw` | float | 0.0f | Camera yaw in degrees |
| +0x04 | `m_pitch` | float | 0.0f | Camera pitch in degrees (clamped ±80°) |
| +0x08 | `m_distance` | float | 6.0f | Viewer camera distance (positive, orthographic) |
| +0x0C | `m_target` | std::array<float,3> | {0,0,0} | Camera look-at target |
| +0x18 | `m_framed` | bool | false | Auto-framing done flag |
| +0x19 | `m_perspective` | bool | false | Perspective vs orthographic projection |
| +0x1D | `m_orbit_distance` | float | 8.0f | Gameplay camera signed orbit distance (may cross zero) |
| +0x21 | `m_vertical_fov_degrees` | float | 55.0f | Vertical FOV |
| +0x25 | `m_near_plane` | float | 0.05f | Near clip plane |
| +0x29 | `m_far_plane` | float | 10000.0f | Far clip plane |
| +0x2D | `m_follow_offset` | std::array<float,3> | {0,1.35,0} | Follow target offset |

`source/gameplay/camera.cpp` implements:
- `update_follow_camera` (line 19): smooth follow of a target position with follow offset
- `orbit_camera` (line 32): yaw/pitch delta input (pitch clamped ±80°)
- `zoom_orbit_camera` (line 39): signed orbit-distance zoom (continuous across zero)
- `calculate_camera_pose` (line 62): computes eye/target/up from yaw/pitch/distance

**No vtable** — `s_camera` is a plain C struct, not a polymorphic class. Camera logic operates via free functions taking `s_camera&`.

## Auto-framing from mesh bounds
Not present in either the generated recomp code or the native runtime camera struct. The native runtime references auto-framing in `README.md` (stage/viewer camera auto-framing) but `camera.hpp`/`camera.cpp` contain no mesh-bounds framing logic — `m_framed` is just a bool flag set by `update_follow_camera`.

## Orbit/pan/zoom controls
Native runtime (`camera.cpp`):
- Yaw/pitch orbit via `orbit_camera` (mouse drag / gamepad stick)
- Signed-distance zoom via `zoom_orbit_camera` (mouse wheel / +/- keys; crosses zero)
- Follow blending via `update_follow_camera` (exponential smoothing, 12.0 s⁻¹)
- `H` key toggles HIT/capsule diagnostics (README.md)

## vtable at +0x00
**Not applicable.** The generated recomp code has no class/vtable concept at all. The native `s_camera` is a non-polymorphic struct with no vtable pointer at `+0x00` (its first member is `m_yaw`, a float).

## Conclusion
No camera class, vtable, or named camera fields exist in the SDBZ recomp generated code. The only camera representation is the native SDL3 runtime's `s_camera` struct in `source/gameplay/camera.hpp`, which is a plain data struct with free-function operators and no vtable. Any camera behavior in the recompiled game is embedded implicitly in raw machine-code math across `unit_*.cpp` files and cannot be mapped to a camera class without source-level type recovery (debug symbols or manual reverse-engineering of field accesses).
