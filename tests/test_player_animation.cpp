#include "gameplay/player_animation.hpp"

#include <iostream>
#include <vector>

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
}

int main()
{
    sdbz::s_player_controller controller;
    require(sdbz::classify_player_animation_state(controller) ==
        sdbz::e_player_animation_state::idle,
        "stationary grounded player selects idle");

    controller.m_velocity = { 1.0f, 0.0f, 0.0f };
    require(sdbz::classify_player_animation_state(controller) ==
        sdbz::e_player_animation_state::walk,
        "moving grounded player selects walk");
    controller.m_running = true;
    require(sdbz::classify_player_animation_state(controller) ==
        sdbz::e_player_animation_state::run,
        "running grounded player selects run");

    controller.m_grounded = false;
    controller.m_velocity[1] = 4.0f;
    require(sdbz::classify_player_animation_state(controller) ==
        sdbz::e_player_animation_state::jump,
        "ascending player selects jump");
    controller.m_velocity[1] = -1.0f;
    require(sdbz::classify_player_animation_state(controller) ==
        sdbz::e_player_animation_state::fall,
        "descending player selects fall");
    require(sdbz::classify_player_animation_state(controller, true) ==
        sdbz::e_player_animation_state::land,
        "landing event overrides the ordinary grounded state");

    std::vector<sdbz::s_animation_clip> clips(4);
    clips[0].m_name = "P01_IDLE";
    clips[1].m_name = "P01_WALK_F";
    clips[2].m_name = "P01_RUN_F";
    clips[3].m_name = "P01_JUMP";
    require(sdbz::find_player_animation_clip(clips,
        sdbz::e_player_animation_state::idle) == 0,
        "clip names are matched case-insensitively");
    require(sdbz::find_player_animation_clip(clips,
        sdbz::e_player_animation_state::walk) == 1,
        "walk state selects a matching clip");
    require(sdbz::find_player_animation_clip(clips,
        sdbz::e_player_animation_state::run) == 2,
        "run state selects a matching clip");
    require(sdbz::find_player_animation_clip(clips,
        sdbz::e_player_animation_state::fall) == 3,
        "fall falls back to the jump clip when no fall clip is present");
    require(!sdbz::find_player_animation_clip({},
        sdbz::e_player_animation_state::land),
        "unmatched state does not invent a clip index");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " player-animation test(s) failed\n";
        return 1;
    }
    std::cout << "Player-animation tests passed\n";
    return 0;
}
