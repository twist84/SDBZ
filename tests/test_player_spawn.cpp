#include "gameplay/player_spawn.hpp"

#include <cmath>
#include <iostream>
#include <string_view>
#include <utility>
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

    sdbz::s_stage_collision_world make_collision_world()
    {
        sdbz::s_stage_collision_world world;
        sdbz::s_stage_collision_polygon floor;
        floor.m_normal = { 0.0f, 1.0f, 0.0f };
        floor.m_bounds_min = { -5.0f, 3.0f, -5.0f };
        floor.m_bounds_max = { 5.0f, 3.0f, 5.0f };
        floor.m_projected_area = 100.0f;
        floor.m_object_name = "arena_floor";
        floor.m_vertices = {
            { -5.0f, 3.0f, -5.0f }, { 5.0f, 3.0f, -5.0f },
            { 5.0f, 3.0f, 5.0f }, { -5.0f, 3.0f, 5.0f }
        };
        world.m_polygons.push_back(floor);
        return world;
    }

    sdbz::s_stage_collision_world make_seamed_collision_world()
    {
        sdbz::s_stage_collision_world world;
        for (int tile = 0; tile < 2; ++tile)
        {
            const float min_x = tile == 0 ? -2.0f : 0.0f;
            const float max_x = tile == 0 ? 0.0f : 2.0f;
            sdbz::s_stage_collision_polygon floor;
            floor.m_normal = { 0.0f, 1.0f, 0.0f };
            floor.m_bounds_min = { min_x, 3.0f, -2.0f };
            floor.m_bounds_max = { max_x, 3.0f, 2.0f };
            floor.m_projected_area = 8.0f;
            floor.m_object_name = "arena_floor_tile";
            floor.m_vertices = {
                { min_x, 3.0f, -2.0f }, { max_x, 3.0f, -2.0f },
                { max_x, 3.0f, 2.0f }, { min_x, 3.0f, 2.0f }
            };
            world.m_polygons.push_back(std::move(floor));
        }
        return world;
    }
}

int main()
{
    constexpr std::string_view config =
        "# stage_key x y z yaw_degrees\n"
        "arena_one 0 3.05 0 90\n"
        "Arena_Two 1 3.05 2 -45 # inline comment\n"
        "bad_entry nope 0 0 0\n"
        "invalid 0 nan 0 0\n";
    const auto overrides = sdbz::parse_player_spawn_overrides(config);
    require(overrides.size() == 2, "spawn config ignores invalid rows and comments");
    require(sdbz::find_player_spawn_override(overrides, "ARENA_ONE") == &overrides[0],
        "stage spawn keys are matched case-insensitively");
    require(sdbz::find_player_spawn_override(overrides, "arena\\two") == nullptr,
        "unmatched stage names do not receive an override");
    require(sdbz::find_player_spawn_override(overrides, "arena_two") == &overrides[1],
        "matching stage override is found");

    const auto world = make_collision_world();
    sdbz::s_player_spawn_override override_entry;
    override_entry.m_stage_key = "arena_one";
    override_entry.m_position = { 0.0f, 3.05f, 0.0f };
    override_entry.m_yaw_degrees = 90.0f;
    const auto spawn = sdbz::resolve_player_spawn_override(world, override_entry);
    require(spawn.has_value(), "valid override resolves against authored HIT floor");
    if (spawn)
    {
        require(std::abs(spawn->m_position[1] - 3.05f) < 0.001f,
            "override keeps player feet at the collision support height");
    }

    override_entry.m_position = { 4.9f, 3.05f, 0.0f };
    require(!sdbz::resolve_player_spawn_override(world, override_entry),
        "override near the edge is rejected when capsule footprint cannot fit");

    const auto seamed_world = make_seamed_collision_world();
    override_entry.m_position = { -0.1f, 3.05f, 0.0f };
    const auto seam_spawn = sdbz::resolve_player_spawn_override(
        seamed_world, override_entry);
    require(seam_spawn.has_value(),
        "spawn override can straddle adjacent HIT polygons without a false edge rejection");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " player-spawn test(s) failed\n";
        return 1;
    }
    std::cout << "Player-spawn tests passed\n";
    return 0;
}
