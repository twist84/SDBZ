#pragma once

#include <cstddef>
#include <cstdint>

namespace sdbz
{

// Slot selector returned by the stage vtable method at +0x28. The parameter is
// the stage object's this-pointer. Returns a spawn slot in the range 0-4.
using stage_slot_selector_fn = std::uint32_t (*)(std::uintptr_t self);

// Runtime stage object reconstructed from the Super Dragon Ball Z PS2 ELF
// disassembly. This is a plain-data mirror of the original 32-bit MIPS object
// layout, so offsetof()/sizeof() match the disassembly on any host.
//
//   +0x00 vtable pointer
//   +0x08 stage ID
//   +0x24 variant bit
//
// Recovered vtable method:
//   +0x28 slot selector returning 0-4 (see s_game_stage_vtable)
struct s_game_stage
{
    std::uint32_t m_vtable = 0;           // +0x00 vtable pointer
    std::uint8_t m_pad_04[0x04] = {};     // +0x04 unattributed
    std::uint32_t m_stage_id = 0;         // +0x08 stage ID
    std::uint8_t m_pad_0c[0x18] = {};     // +0x0c..0x23 unattributed
    std::uint32_t m_variant_bit = 0;      // +0x24 variant bit
};

static_assert(offsetof(s_game_stage, m_vtable) == 0x00,
    "s_game_stage vtable pointer must remain at +0x00");
static_assert(offsetof(s_game_stage, m_stage_id) == 0x08,
    "s_game_stage stage ID must remain at +0x08");
static_assert(offsetof(s_game_stage, m_variant_bit) == 0x24,
    "s_game_stage variant bit must remain at +0x24");
static_assert(sizeof(s_game_stage) == 0x28,
    "s_game_stage must remain 0x28 bytes");

struct s_game_stage_vtable
{
    std::uint8_t m_pad_00[0x28] = {};        // +0x00..0x27 unattributed
    std::uint32_t m_slot_selector = 0;       // +0x28 slot selector (0-4)
};

static_assert(offsetof(s_game_stage_vtable, m_slot_selector) == 0x28,
    "s_game_stage_vtable slot selector must remain at +0x28");

} // namespace sdbz
