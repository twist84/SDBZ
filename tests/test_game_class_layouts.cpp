#include "gameplay/fighter.hpp"
#include "gameplay/fighter_side.hpp"
#include "gameplay/game_session.hpp"
#include "gameplay/game_stage.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

namespace
{
    int g_failures = 0;

    void require(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++g_failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }
} // namespace

int main()
{
    // Stage
    require(offsetof(sdbz::s_game_stage, m_vtable) == 0x00,
        "game_stage vtable at +0x00");
    require(offsetof(sdbz::s_game_stage, m_stage_id) == 0x08,
        "game_stage stage ID at +0x08");
    require(offsetof(sdbz::s_game_stage, m_variant_bit) == 0x24,
        "game_stage variant bit at +0x24");
    require(sizeof(sdbz::s_game_stage) == 0x28, "game_stage size 0x28");

    require(offsetof(sdbz::s_game_stage_vtable, m_slot_selector) == 0x28,
        "game_stage_vtable slot selector at +0x28");

    // Fighter
    require(offsetof(sdbz::s_fighter, m_vtable) == 0x00,
        "fighter vtable at +0x00");
    require(offsetof(sdbz::s_fighter, m_spawn_position) == 0x510,
        "fighter spawn vec4 at +0x510");
    require(offsetof(sdbz::s_fighter, m_flags) == 0x520,
        "fighter flags bitmask at +0x520");
    require(offsetof(sdbz::s_fighter, m_side_controller_ptr) == 0x534,
        "fighter side sub-object pointer at +0x534");
    require(offsetof(sdbz::s_fighter, m_vtable_table_index) == 0x538,
        "fighter vtable table index at +0x538");
    require(offsetof(sdbz::s_fighter, m_vtable_table_index_mirror) == 0x53c,
        "fighter vtable table index mirror at +0x53c");
    require(sizeof(sdbz::s_fighter) == 0x540, "fighter size 0x540");

    require(offsetof(sdbz::s_fighter_vtable, m_set_position) == 0x40,
        "fighter_vtable position setter at +0x40");
    require(offsetof(sdbz::s_fighter_vtable, m_set_orientation) == 0x4c,
        "fighter_vtable orientation setter at +0x4c");
    require(offsetof(sdbz::s_fighter_vtable, m_state_configuration) == 0x78,
        "fighter_vtable state/configuration at +0x78");
    require(offsetof(sdbz::s_fighter_vtable, m_post_placement) == 0xac,
        "fighter_vtable post-placement at +0xac");

    // Session
    require(offsetof(sdbz::s_game_session, m_stage) == 0x58,
        "session stage at +0x58");
    require(offsetof(sdbz::s_game_session, m_fighter1) == 0x60,
        "session fighter1 at +0x60");
    require(offsetof(sdbz::s_game_session, m_fighter2) == 0x64,
        "session fighter2 at +0x64");
    require(sizeof(sdbz::s_game_session) == 0x68, "session size 0x68");

    // Side selector sub-object
    require(offsetof(sdbz::s_fighter_side, m_side_selector) == 0x08,
        "fighter_side side selector at +0x08");
    require(sizeof(sdbz::s_fighter_side) == 0x0c, "fighter_side size 0x0c");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " game-class layout test(s) failed\n";
        return 1;
    }
    std::cout << "Game-class layout tests passed\n";
    return 0;
}
