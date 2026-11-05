# Game class reconstruction — Super Dragon Ball Z

Reconstructed runtime game classes from ELF disassembly findings (PS2 MIPS R5900,
32-bit object layout). These are plain-data layout mirrors: `offsetof()` and
`sizeof()` match the disassembly on any host. No ELF files were modified; no
audio or rendering code was touched.

## Source files

- `source/gameplay/game_stage.hpp/.cpp` — `sdbz::s_game_stage`
- `source/gameplay/fighter.hpp/.cpp` — `sdbz::s_fighter` (+ `sdbz::s_fighter_vtable`)
- `source/gameplay/game_session.hpp/.cpp` — `sdbz::s_game_session`
- `source/gameplay/fighter_side.hpp/.cpp` — `sdbz::s_fighter_side`
- `tests/test_game_class_layouts.cpp` — compile-time `static_assert` + run-time `offsetof`/`sizeof` verification

Address-sized fields (vtable pointer, pointers, indices) use `std::uint32_t`
with explicit padding so the original 32-bit offsets are preserved exactly on
64-bit hosts. Vtable slots hold 32-bit function addresses; recovered signatures
are provisional.

## s_game_stage

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| +0x00 | `m_vtable` | `std::uint32_t` | vtable pointer |
| +0x08 | `m_stage_id` | `std::uint32_t` | stage ID |
| +0x24 | `m_variant_bit` | `std::uint32_t` | variant bit |

`sizeof == 0x28`.

### s_game_stage_vtable

| Offset | Slot | Notes |
|--------|------|-------|
| +0x28 | `m_slot_selector` | slot selector returning 0-4 |

## s_fighter

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| +0x00 | `m_vtable` | `std::uint32_t` | vtable pointer |
| +0x510 | `m_spawn_position` | `std::array<float, 4>` | spawn vec4 |
| +0x520 | `m_flags` | `std::uint32_t` | flags bitmask |
| +0x534 | `m_side_controller_ptr` | `std::uint32_t` | pointer to side/controller sub-object |
| +0x538 | `m_vtable_table_index` | `std::uint32_t` | vtable table index |
| +0x53c | `m_vtable_table_index_mirror` | `std::uint32_t` | mirrors +0x538 |

`sizeof == 0x540`.

### s_fighter_vtable

| Offset | Slot | Notes |
|--------|------|-------|
| +0x40 | `m_set_position` | position setter |
| +0x4c | `m_set_orientation` | orientation/rotation setter |
| +0x78 | `m_state_configuration` | state/configuration |
| +0xac | `m_post_placement` | post-placement |

## s_game_session

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| +0x58 | `m_stage` | `std::uint32_t` | stage pointer |
| +0x60 | `m_fighter1` | `std::uint32_t` | fighter 1 pointer |
| +0x64 | `m_fighter2` | `std::uint32_t` | fighter 2 pointer |

`sizeof == 0x68`.

## s_fighter_side

Pointed to by `s_fighter::m_side_controller_ptr` (+0x534).

| Offset | Field | Type | Notes |
|--------|-------|------|-------|
| +0x08 | `m_side_selector` | `std::uint32_t` | binary side selector (0 = P1, 1 = P2) |

`sizeof == 0x0c`.

## Verification

Each header carries `static_assert`s for every documented offset and size; the
new sources are compiled into `sdbz_core`, and `tests/test_game_class_layouts.cpp`
re-checks all offsets/sizes at run time (registered as the `game_class_layout`
CTest).
