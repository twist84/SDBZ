#pragma once

#include <cstddef>
#include <cstdint>

namespace sdbz
{

// Session/owner object reconstructed from the Super Dragon Ball Z PS2 ELF
// disassembly. Plain-data mirror of the original 32-bit MIPS object layout.
//
//   +0x58 stage pointer
//   +0x60 fighter 1 pointer
//   +0x64 fighter 2 pointer
struct s_game_session
{
    std::uint8_t m_pad_00[0x58] = {};     // +0x00..0x57 unattributed
    std::uint32_t m_stage = 0;            // +0x58 stage pointer
    std::uint8_t m_pad_5c[0x04] = {};     // +0x5c..0x5f unattributed
    std::uint32_t m_fighter1 = 0;         // +0x60 fighter 1 pointer
    std::uint32_t m_fighter2 = 0;         // +0x64 fighter 2 pointer
};

static_assert(offsetof(s_game_session, m_stage) == 0x58,
    "s_game_session stage pointer must remain at +0x58");
static_assert(offsetof(s_game_session, m_fighter1) == 0x60,
    "s_game_session fighter 1 pointer must remain at +0x60");
static_assert(offsetof(s_game_session, m_fighter2) == 0x64,
    "s_game_session fighter 2 pointer must remain at +0x64");
static_assert(sizeof(s_game_session) == 0x68,
    "s_game_session must remain 0x68 bytes");

} // namespace sdbz
