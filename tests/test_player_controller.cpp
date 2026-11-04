#include "gameplay/player_controller.hpp"
#include "core/stage_collision.hpp"

#include <cmath>
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
}

int main()
{
    sdbz::s_player_controller controller;
    sdbz::s_player_input input{};
    input.m_move_forward = 1.0f;
    sdbz::update_player_controller(controller, input, 0.1f);
    require(controller.m_position[2] < 0.0f,
        "forward input moves along negative Z");
    require(controller.m_position[0] == 0.0f,
        "forward input does not drift on X");

    sdbz::s_player_input camera_input{};
    camera_input.m_move_forward = 1.0f;
    auto world_input = sdbz::make_camera_relative_player_input(camera_input, 90.0f);
    require(std::abs(world_input.m_move_x + 1.0f) < 0.001f &&
        std::abs(world_input.m_move_forward) < 0.001f,
        "camera-relative forward input follows camera heading at 90 degrees");
    camera_input = {};
    camera_input.m_move_x = 1.0f;
    world_input = sdbz::make_camera_relative_player_input(camera_input, 90.0f);
    require(std::abs(world_input.m_move_forward - 1.0f) < 0.001f,
        "camera-relative right input is perpendicular to camera forward");

    sdbz::s_player_controller fixed_a;
    sdbz::s_player_controller fixed_b;
    sdbz::s_player_input fixed_input{};
    fixed_input.m_move_forward = 1.0f;
    sdbz::update_player_controller(fixed_a, fixed_input, 0.1f);
    for (int frame = 0; frame < 6; ++frame)
    {
        sdbz::update_player_controller(fixed_b, fixed_input, 1.0f / 60.0f);
    }
    require(std::abs(fixed_a.m_position[2] - fixed_b.m_position[2]) < 0.001f,
        "fixed-step movement is independent of render-frame chunking");

    sdbz::s_player_controller buffered_jump;
    sdbz::s_player_input jump_edge{};
    jump_edge.m_jump_pressed = true;
    sdbz::update_player_controller(buffered_jump, jump_edge, 0.005f);
    jump_edge.m_jump_pressed = false;
    sdbz::update_player_controller(buffered_jump, jump_edge, 0.012f);
    require(!buffered_jump.m_grounded && buffered_jump.m_position[1] > 0.0f,
        "jump edge survives until the next fixed simulation tick");

    controller = {};
    input = {};
    input.m_move_x = 1.0f;
    input.m_move_forward = 1.0f;
    input.m_run = true;
    for (int frame = 0; frame < 8; ++frame)
    {
        sdbz::update_player_controller(controller, input, 0.1f);
    }
    const float horizontal_speed = std::sqrt(
        controller.m_velocity[0] * controller.m_velocity[0] +
        controller.m_velocity[2] * controller.m_velocity[2]);
    require(horizontal_speed <= controller.m_run_speed + 0.001f,
        "diagonal run speed is normalized");
    require(controller.m_velocity[0] > 0.0f && controller.m_velocity[2] < 0.0f,
        "diagonal input moves along both axes");

    input = {};
    input.m_jump_pressed = true;
    sdbz::update_player_controller(controller, input, 1.0f / 60.0f);
    require(!controller.m_grounded && controller.m_position[1] > 0.0f,
        "jump leaves the ground");
    input.m_jump_pressed = false;
    for (int frame = 0; frame < 60; ++frame)
    {
        sdbz::update_player_controller(controller, input, 0.1f);
    }
    require(controller.m_grounded &&
        controller.m_position[1] == controller.m_ground_height,
        "gravity returns the player to the ground");

    // Stage collision must keep the controller grounded on narrow HIT floor
    // strips. A capsule's bottom contact is a point at its centreline; requiring
    // its complete projected radius to overlap a polygon makes it fall through.
    sdbz::s_stage_collision_world narrow_floor;
    sdbz::s_stage_collision_polygon narrow_floor_polygon;
    narrow_floor_polygon.m_normal = { 0.0f, 1.0f, 0.0f };
    narrow_floor_polygon.m_bounds_min = { -0.12f, 0.0f, -4.0f };
    narrow_floor_polygon.m_bounds_max = { 0.12f, 0.0f, 4.0f };
    narrow_floor_polygon.m_projected_area = 1.92f;
    narrow_floor_polygon.m_vertices = {
        { -0.12f, 0.0f, -4.0f }, { 0.12f, 0.0f, -4.0f },
        { 0.12f, 0.0f, 4.0f }, { -0.12f, 0.0f, 4.0f }
    };
    narrow_floor.m_polygons.push_back(std::move(narrow_floor_polygon));
    sdbz::build_stage_collision_spatial_index(narrow_floor);

    sdbz::s_player_controller collision_controller;
    collision_controller.m_position = { 0.0f, 0.05f, 0.0f };
    collision_controller.m_grounded = true;
    sdbz::s_player_input no_movement{};
    for (int frame = 0; frame < 120; ++frame)
    {
        sdbz::update_player_controller(collision_controller, no_movement,
            1.0f / 60.0f, &narrow_floor);
    }
    require(collision_controller.m_grounded &&
        std::abs(collision_controller.m_position[1] - 0.05f) < 0.001f,
        "stage collision controller remains grounded on a narrow HIT floor polygon");

    // The collision-aware controller must also land after a jump and remain
    // attached to the same surface instead of reverting to flat-ground mode.
    sdbz::s_player_input collision_jump{};
    collision_jump.m_jump_pressed = true;
    sdbz::update_player_controller(collision_controller, collision_jump,
        1.0f / 60.0f, &narrow_floor);
    collision_jump.m_jump_pressed = false;
    require(!collision_controller.m_grounded && collision_controller.m_velocity[1] > 0.0f,
        "collision-aware jump leaves the HIT floor");
    for (int frame = 0; frame < 120; ++frame)
    {
        sdbz::update_player_controller(collision_controller, collision_jump,
            1.0f / 60.0f, &narrow_floor);
    }
    require(collision_controller.m_grounded &&
        std::abs(collision_controller.m_position[1] - 0.05f) < 0.001f,
        "collision-aware player lands back on the HIT floor");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " player-controller test(s) failed\n";
        return 1;
    }
    std::cout << "Player-controller tests passed\n";
    return 0;
}
