#pragma once

#include <cstddef>
#include <cstdint>

namespace sdbz
{

// Side/controller sub-object pointed to by s_fighter::m_side_controller_ptr.
// Plain-data mirror of the original 32-bit MIPS sub-object layout.
//
//   +0x08 binary side selector (0 = P1, 1 = P2)
struct s_fighter_side
{
    std::uint8_t m_pad_00[0x08] = {};     // +0x00..0x07 unattributed
    std::uint32_t m_side_selector = 0;    // +0x08 binary side selector P1/P2
};

static_assert(offsetof(s_fighter_side, m_side_selector) == 0x08,
    "s_fighter_side side selector must remain at +0x08");
static_assert(sizeof(s_fighter_side) == 0x0c,
    "s_fighter_side must remain 0x0c bytes");

} // namespace sdbz
