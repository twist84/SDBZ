#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sdbz
{

// Provisional recovered virtual-method signatures for the fighter vtable. The
// first parameter is the fighter object's this-pointer; parameter types beyond
// that are pending full ELF signature recovery.
using fighter_position_setter_fn = void (*)(std::uintptr_t self,
    const std::array<float, 3>& position);
using fighter_orientation_setter_fn = void (*)(std::uintptr_t self,
    const std::array<float, 3>& euler_radians);
using fighter_state_config_fn = void (*)(std::uintptr_t self,
    std::uint32_t configuration);
using fighter_post_placement_fn = void (*)(std::uintptr_t self);

// Runtime fighter/player object reconstructed from the Super Dragon Ball Z PS2
// ELF disassembly. Plain-data mirror of the original 32-bit MIPS object layout.
//
//   +0x00 vtable pointer
//   +0x510 spawn vec4
//   +0x520 flags bitmask
//   +0x534 pointer to side/controller sub-object
//   +0x538 vtable table index
//   +0x53c mirrors +0x538
//
// Recovered vtable methods:
//   +0x40 position setter
//   +0x4c orientation/rotation setter
//   +0x78 state/configuration
//   +0xac post-placement
struct s_fighter
{
    std::uint32_t m_vtable = 0;                        // +0x00 vtable pointer
    std::uint8_t m_pad_04[0x50c] = {};                 // +0x04..0x50f unattributed
    std::array<float, 4> m_spawn_position{};           // +0x510 spawn vec4
    std::uint32_t m_flags = 0;                         // +0x520 flags bitmask
    std::uint8_t m_pad_524[0x10] = {};                 // +0x524..0x533 unattributed
    std::uint32_t m_side_controller_ptr = 0;           // +0x534 side sub-object ptr
    std::uint32_t m_vtable_table_index = 0;            // +0x538 vtable table index
    std::uint32_t m_vtable_table_index_mirror = 0;     // +0x53c mirrors +0x538
};

static_assert(offsetof(s_fighter, m_vtable) == 0x00,
    "s_fighter vtable pointer must remain at +0x00");
static_assert(offsetof(s_fighter, m_spawn_position) == 0x510,
    "s_fighter spawn vec4 must remain at +0x510");
static_assert(offsetof(s_fighter, m_flags) == 0x520,
    "s_fighter flags bitmask must remain at +0x520");
static_assert(offsetof(s_fighter, m_side_controller_ptr) == 0x534,
    "s_fighter side sub-object pointer must remain at +0x534");
static_assert(offsetof(s_fighter, m_vtable_table_index) == 0x538,
    "s_fighter vtable table index must remain at +0x538");
static_assert(offsetof(s_fighter, m_vtable_table_index_mirror) == 0x53c,
    "s_fighter vtable table index mirror must remain at +0x53c");
static_assert(sizeof(s_fighter) == 0x540,
    "s_fighter must remain 0x540 bytes");

struct s_fighter_vtable
{
    std::uint8_t m_pad_00[0x40] = {};                  // +0x00..0x3f unattributed
    std::uint32_t m_set_position = 0;                  // +0x40 position setter
    std::uint8_t m_pad_44[0x08] = {};                  // +0x44..0x4b unattributed
    std::uint32_t m_set_orientation = 0;               // +0x4c orientation setter
    std::uint8_t m_pad_50[0x28] = {};                  // +0x50..0x77 unattributed
    std::uint32_t m_state_configuration = 0;           // +0x78 state/configuration
    std::uint8_t m_pad_7c[0x30] = {};                  // +0x7c..0xab unattributed
    std::uint32_t m_post_placement = 0;                // +0xac post-placement
};

static_assert(offsetof(s_fighter_vtable, m_set_position) == 0x40,
    "s_fighter_vtable position setter must remain at +0x40");
static_assert(offsetof(s_fighter_vtable, m_set_orientation) == 0x4c,
    "s_fighter_vtable orientation setter must remain at +0x4c");
static_assert(offsetof(s_fighter_vtable, m_state_configuration) == 0x78,
    "s_fighter_vtable state/configuration must remain at +0x78");
static_assert(offsetof(s_fighter_vtable, m_post_placement) == 0xac,
    "s_fighter_vtable post-placement must remain at +0xac");

} // namespace sdbz
