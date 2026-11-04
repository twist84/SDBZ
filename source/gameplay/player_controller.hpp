#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace sdbz
{
    struct s_stage_collision_world;

    struct s_player_input
    {
        float m_move_x = 0.0f;
        float m_move_forward = 0.0f;
        bool m_run = false;
        bool m_jump_pressed = false;
    };

    struct s_player_controller
    {
        std::array<float, 3> m_position{};
        std::array<float, 3> m_velocity{};
        std::array<float, 3> m_ground_normal{ 0.0f, 1.0f, 0.0f };
        std::size_t m_ground_polygon_index = static_cast<std::size_t>(-1);
        std::string m_ground_object_name;
        float m_yaw_degrees = 0.0f;
        float m_ground_height = 0.0f;
        float m_walk_speed = 8.0f;
        float m_run_speed = 14.0f;
        float m_jump_speed = 7.5f;
        float m_gravity = 28.0f;
        float m_acceleration = 36.0f;
        float m_deceleration = 42.0f;
        float m_collision_foot_offset = 0.05f;
        float m_collision_capsule_radius = 0.25f;
        float m_collision_capsule_height = 1.75f;
        // At run speed, one fixed 60 Hz tick climbs about 0.40 world units
        // on a 60-degree ramp. Keep the step allowance slightly above that.
        float m_collision_step_height = 0.42f;
        float m_collision_step_down = 0.65f;
        float m_max_walkable_slope_degrees = 60.0f;
        bool m_grounded = true;
        bool m_running = false;
        bool m_landed_this_frame = false;
        float m_fixed_time_accumulator = 0.0f;
        bool m_jump_pressed_pending = false;
    };

    // Rotates local movement input into world-space directions for the active
    // gameplay camera yaw. The controller's forward axis is negative Z.
    [[nodiscard]] s_player_input make_camera_relative_player_input(
        const s_player_input& input, float camera_yaw_degrees);

    // Legacy flat-ground overload for model-only previews. Stage gameplay must
    // use the collision-world overload below.
    void update_player_controller(s_player_controller& controller,
        const s_player_input& input, float delta_seconds);

    void update_player_controller(s_player_controller& controller,
        const s_player_input& input, float delta_seconds,
        const s_stage_collision_world* collision_world);
}
