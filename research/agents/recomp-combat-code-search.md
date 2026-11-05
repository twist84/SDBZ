# Recompiled Code — Combat/Hitbox Code Search Results

## 1. Summary

**No combat/hitbox-related code exists in any of the 240 generated EE units** (`unit_0000.cpp` through `unit_00F8.cpp`) or the 9 VU1 microcode files (`vu1_000.cpp` through `vu1_008.cpp`) in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`. This is likely because combat/hitbox logic on the PS2 was handled either by the IOP (audio/physics/peripherals) or through VU vector math that doesn't use combat-specific terminology, or it may be in a layer not covered by this decompilation.

## 2. Search Results by Category

### 2.1 Hitboxes / Attack Boxes — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bhitbox\b` | No matches |
| `\battack_box\b` | No matches |
| `\bdamage_box\b` | No matches |
| `\bhit_frame\b` | No matches |
| `\bactive_frame\b` | No matches |

### 2.2 Attack Moves — NOT FOUND (false positives only)

| Search term | Result |
|-------------|--------|
| `\bpunch\b` | No matches |
| `\bkick\b` | Only `vu::kick()` — VU instruction (VFPU `kick` opcode), not combat kicking |
| `\bki_blast\b` | No matches |
| `\bspecial_move\b` | No matches |
| `\bcombo\b` | No matches |
| `\battack\b` | Only `vu::kick()` false positives (same as above) |

### 2.3 Hurtboxes / Vulnerability — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bhurtbox\b` | No matches |
| `\bvulnerable_box\b` | No matches |
| `\bcollision_hit\b` | No matches |
| `\bvulnerable\b` | No matches |

### 2.4 Damage Calculation — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bdamage\b` | No matches |
| `\bhealth\b` | No matches |
| `\bki_drain\b` | No matches |
| `\bstun\b` | No matches |
| `\bhp\b` | No matches |
| `\blife\b` | No matches |

### 2.5 Attack Animation — NOT FOUND (false positives only)

| Search term | Result |
|-------------|--------|
| `\battack_anim\b` | No matches |
| `\bmove_anim\b` | No matches |
| `\bspecial_anim\b` | No matches |
| `\banim\b` | Only `vu::move()` — VU instruction (VFPU `move` opcode), not animation |
| `\bframe\b` | No matches |
| `\bmove\b` | Only `vu::move()` false positives (same as above) |

### 2.6 Additional Combat Terms — NOT FOUND

| Search term | Result |
|-------------|--------|
| `\bguard\b` | No matches |
| `\bblock\b` | No matches |
| `\bknockdown\b` | No matches |
| `\brecovery\b` | No matches |
| `\bknockback\b` | No matches |
| `\bhit\b` | No matches |
| `\bcollide\b` | No matches |
| `\bhurt\b` | No matches |
| `\binjury\b` | No matches |

### 2.7 VU1 Microcode Files — VECTOR MATH ONLY

The 9 VU1 microcode files (`vu1_000.cpp`–`vu1_008.cpp`) contain only VFPU vector math instructions:
- `vu::kick(1, iN)` — VU `kick` instruction (synchronizes VU with EE), not combat kicking
- `vu::move(c.vu0, ...)` — VU `move` instruction (copies between VU registers), not animation movement

No combat-specific logic exists in these files.

## 3. Files Searched

All 249 files in `C:\Dev\ps2-web-recomp\sdbz\generated\SuperDragonBallZ\`:
- 240 EE units: `unit_0000.cpp` through `unit_00F8.cpp` (VA `0x00100000`–`0x004E0000`)
- 9 VU1 microcode units: `vu1_000.cpp` through `vu1_008.cpp`
- `table.cpp` (unit address mapping)
- `units.hpp` (unit function declarations)

## 4. Why No Combat Code Exists in the Decompiled Units

Combat/hitbox logic on the PS2 may be handled by:

1. **IOP-side code** — Physics, collision detection, and hitbox/hurtbox processing may run on the IOP, not the EE. The `ps2-web-recomp` project only decompiles EE code.
2. **VU vector math without combat terminology** — Hitbox collision may be done through raw VU vector dot/cross products without named "hitbox" or "hurtbox" variables. The VU1 microcode contains only generic vector math (`kick`, `move` instructions).
3. **Different binary layer** — Combat logic may be in a different executable segment or overlay not covered by the 240-unit decompilation.
4. **Library functions** — Combat functions may be called through a library with non-obvious names that don't match the search terms.

## 5. Recommendations for Future Combat Research

If combat/hitbox research is needed:

1. **Search for collision math patterns** — Look for VU dot product, cross product, and distance calculations that could represent hitbox/hurtbox intersection tests.
2. **Examine VU1 microcode more closely** — The `vu1_*.cpp` files may contain vector math used for collision detection, even without combat-specific names.
3. **IOP-side decompilation** — Collision and hitbox processing may be on the IOP.
4. **Function call graph analysis** — Trace calls from animation state machines to find where hit events are generated.
5. **Look for state machine transitions** — Combat moves (punch, kick, special) may be represented as animation state IDs rather than named functions.
