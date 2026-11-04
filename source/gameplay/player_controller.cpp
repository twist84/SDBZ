#include "gameplay/player_controller.hpp"

#include "core/stage_collision.hpp"

#include <algorithm>
#include <cmath>

namespace sdbz
{
    namespace
    {
        constexpr float k_radians_to_degrees = 57.295779513082320876f;
        constexpr float k_degrees_to_radians = 0.01745329251994329577f;
        constexpr float k_fixed_step = 1.0f / 60.0f;
        constexpr float k_accumulator_epsilon = 1.0e-6f;

        struct s_movement_target
        {
            float m_x = 0.0f;
            float m_z = 0.0f;
        };

        [[nodiscard]] s_movement_target calculate_target_velocity(
            const s_player_controller& controller, const s_player_input& input)
        {
            float move_x = std::clamp(input.m_move_x, -1.0f, 1.0f);
            float move_forward = std::clamp(input.m_move_forward, -1.0f, 1.0f);
            const float input_length = std::sqrt(move_x * move_x + move_forward * move_forward);
            if (input_length > 1.0f)
            {
                move_x /= input_length;
                move_forward /= input_length;
            }
            const float speed = input.m_run ? controller.m_run_speed : controller.m_walk_speed;
            return { move_x * speed, -move_forward * speed };
        }

        void update_horizontal_velocity(s_player_controller& controller,
            const s_player_input& input, const s_movement_target& target, const float dt)
        {
            const bool has_movement_input =
                input.m_move_x != 0.0f || input.m_move_forward != 0.0f;
            const float rate = has_movement_input
                ? controller.m_acceleration : controller.m_deceleration;
            const float max_delta = std::max(rate, 0.0f) * dt;
            const float delta_x = target.m_x - controller.m_velocity[0];
            const float delta_z = target.m_z - controller.m_velocity[2];
            const float delta_length = std::sqrt(delta_x * delta_x + delta_z * delta_z);
            if (delta_length <= max_delta || delta_length <= 1.0e-6f)
            {
                controller.m_velocity[0] = target.m_x;
                controller.m_velocity[2] = target.m_z;
            }
            else
            {
                const float scale = max_delta / delta_length;
                controller.m_velocity[0] += delta_x * scale;
                controller.m_velocity[2] += delta_z * scale;
            }
        }

        void update_facing(s_player_controller& controller)
        {
            const float horizontal_speed_sq =
                controller.m_velocity[0] * controller.m_velocity[0] +
                controller.m_velocity[2] * controller.m_velocity[2];
            if (horizontal_speed_sq > 1.0e-4f)
            {
                controller.m_yaw_degrees = std::atan2(controller.m_velocity[0],
                    controller.m_velocity[2]) * k_radians_to_degrees;
            }
        }

        void update_flat_ground_step(s_player_controller& controller,
            const s_player_input& input, const float dt)
        {
            const s_movement_target target = calculate_target_velocity(controller, input);
            update_horizontal_velocity(controller, input, target, dt);
            if (input.m_jump_pressed && controller.m_grounded)
            {
                controller.m_velocity[1] = controller.m_jump_speed;
                controller.m_grounded = false;
            }
            if (!controller.m_grounded)
            {
                controller.m_velocity[1] -= std::max(controller.m_gravity, 0.0f) * dt;
            }
            controller.m_position[0] += controller.m_velocity[0] * dt;
            controller.m_position[1] += controller.m_velocity[1] * dt;
            controller.m_position[2] += controller.m_velocity[2] * dt;
            if (controller.m_position[1] <= controller.m_ground_height)
            {
                controller.m_landed_this_frame =
                    controller.m_landed_this_frame || !controller.m_grounded;
                controller.m_position[1] = controller.m_ground_height;
                controller.m_velocity[1] = 0.0f;
                controller.m_grounded = true;
            }
            update_facing(controller);
        }

        void update_collision_step(s_player_controller& controller,
            const s_player_input& input, const s_movement_target& target,
            const s_stage_collision_world& collision_world, const float dt)
        {
            const float previous_y = controller.m_position[1];
            bool was_grounded = controller.m_grounded;
            if (was_grounded)
            {
                const auto support = query_stage_ground_capsule(collision_world,
                    controller.m_position[0], controller.m_position[2],
                    previous_y - controller.m_collision_foot_offset,
                    controller.m_collision_capsule_radius,
                    controller.m_collision_step_height,
                    controller.m_collision_step_down,
                    controller.m_max_walkable_slope_degrees);
                if (support)
                {
                    controller.m_ground_normal = support->m_normal;
                    controller.m_ground_polygon_index = support->m_collision_polygon_index;
                    controller.m_ground_object_name = support->m_object_name;
                    controller.m_position[1] = support->m_position[1] +
                        controller.m_collision_foot_offset;
                }
                else
                {
                    was_grounded = false;
                    controller.m_grounded = false;
                    controller.m_ground_polygon_index = static_cast<std::size_t>(-1);
                    controller.m_ground_object_name.clear();
                }
            }

            update_horizontal_velocity(controller, input, target, dt);
            if (input.m_jump_pressed && was_grounded)
            {
                controller.m_velocity[1] = controller.m_jump_speed;
                controller.m_grounded = false;
                was_grounded = false;
            }
            else if (!was_grounded)
            {
                controller.m_velocity[1] -= std::max(controller.m_gravity, 0.0f) * dt;
            }

            const float desired_x = controller.m_position[0] + controller.m_velocity[0] * dt;
            const float desired_z = controller.m_position[2] + controller.m_velocity[2] * dt;
            float next_y = controller.m_position[1] + controller.m_velocity[1] * dt;
            const auto resolved_horizontal = resolve_stage_horizontal_movement(
                collision_world, controller.m_position[0], controller.m_position[2],
                desired_x, desired_z,
                controller.m_position[1] - controller.m_collision_foot_offset,
                controller.m_collision_capsule_radius,
                controller.m_collision_capsule_height,
                controller.m_max_walkable_slope_degrees);
            float next_x = resolved_horizontal[0];
            float next_z = resolved_horizontal[1];
            if (next_y > controller.m_position[1] + 1.0e-5f &&
                !is_stage_capsule_clear(collision_world, next_x, next_z,
                    next_y - controller.m_collision_foot_offset,
                    controller.m_collision_capsule_radius,
                    controller.m_collision_capsule_height,
                    controller.m_max_walkable_slope_degrees))
            {
                // Uphill momentum can move the capsule into the rising ramp
                // during the first jump frames. In that case hold horizontal
                // movement for this tick while allowing the jump to rise; the
                // player can move forward again once the capsule clears the
                // surface. A real overhead obstruction also blocks the
                // current column, in which case upward movement is stopped.
                const bool current_column_clear = is_stage_capsule_clear(
                    collision_world, controller.m_position[0], controller.m_position[2],
                    next_y - controller.m_collision_foot_offset,
                    controller.m_collision_capsule_radius,
                    controller.m_collision_capsule_height,
                    controller.m_max_walkable_slope_degrees);
                if (current_column_clear)
                {
                    next_x = controller.m_position[0];
                    next_z = controller.m_position[2];
                }
                else
                {
                    // Stop upward movement at an actual overhead HIT surface.
                    next_y = controller.m_position[1];
                    controller.m_velocity[1] = 0.0f;
                }
            }
            const float support_reference_y = std::max(previous_y, next_y) -
                controller.m_collision_foot_offset;
            const float max_drop = was_grounded
                ? controller.m_collision_step_down : 1000000.0f;

            // Resolve support at the destination, not at the highest point
            // anywhere along the swept path. Using an intermediate point here
            // can keep the player grounded after walking off an edge or pull
            // the player up onto a platform before reaching it. Vertical
            // crossing is still detected by the previous/next feet heights.
            const auto best_support = query_stage_ground_capsule(collision_world,
                next_x, next_z, support_reference_y,
                controller.m_collision_capsule_radius,
                controller.m_collision_step_height, max_drop,
                controller.m_max_walkable_slope_degrees);

            controller.m_position[0] = next_x;
            controller.m_position[2] = next_z;
            controller.m_position[1] = next_y;
            controller.m_grounded = false;

            if (best_support)
            {
                const float support_y = best_support->m_position[1] +
                    controller.m_collision_foot_offset;
                const bool walked_onto_surface = was_grounded &&
                    controller.m_velocity[1] <= 0.0f &&
                    support_y >= previous_y - controller.m_collision_step_down - 0.01f &&
                    support_y <= previous_y + controller.m_collision_step_height + 0.01f;
                const bool landed = controller.m_velocity[1] <= 0.0f &&
                    previous_y >= support_y - 0.02f && next_y <= support_y + 0.02f;
                if (walked_onto_surface || landed)
                {
                    if (landed && !was_grounded)
                    {
                        controller.m_landed_this_frame = true;
                    }
                    controller.m_position[1] = support_y;
                    controller.m_velocity[1] = 0.0f;
                    controller.m_ground_normal = best_support->m_normal;
                    controller.m_ground_polygon_index = best_support->m_collision_polygon_index;
                    controller.m_ground_object_name = best_support->m_object_name;
                    controller.m_grounded = true;
                }
            }
            update_facing(controller);
        }
    }

    s_player_input make_camera_relative_player_input(
        const s_player_input& input, const float camera_yaw_degrees)
    {
        const float yaw = camera_yaw_degrees * k_degrees_to_radians;
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        s_player_input result = input;
        result.m_move_x = cosine * input.m_move_x - sine * input.m_move_forward;
        result.m_move_forward = sine * input.m_move_x + cosine * input.m_move_forward;
        return result;
    }

    void update_player_controller(s_player_controller& controller,
        const s_player_input& input, const float delta_seconds)
    {
        update_player_controller(controller, input, delta_seconds, nullptr);
    }

    void update_player_controller(s_player_controller& controller,
        const s_player_input& input, const float delta_seconds,
        const s_stage_collision_world* collision_world)
    {
        controller.m_landed_this_frame = false;
        controller.m_running = input.m_run;
        if (input.m_jump_pressed)
        {
            controller.m_jump_pressed_pending = true;
        }

        const float frame_dt = std::clamp(delta_seconds, 0.0f, 0.25f);
        if (frame_dt <= 0.0f)
        {
            return;
        }

        // Simulate at a stable 60 Hz regardless of render cadence. Clamp the
        // backlog to the maximum frame delta accepted by the platform loop.
        controller.m_fixed_time_accumulator = std::min(
            controller.m_fixed_time_accumulator + frame_dt, 0.25f);
        const s_movement_target target = calculate_target_velocity(controller, input);
        while (controller.m_fixed_time_accumulator + k_accumulator_epsilon >= k_fixed_step)
        {
            s_player_input step_input = input;
            step_input.m_jump_pressed = controller.m_jump_pressed_pending;
            controller.m_jump_pressed_pending = false;

            if (collision_world)
            {
                update_collision_step(controller, step_input, target,
                    *collision_world, k_fixed_step);
            }
            else
            {
                update_flat_ground_step(controller, step_input, k_fixed_step);
            }
            controller.m_fixed_time_accumulator = std::max(
                0.0f, controller.m_fixed_time_accumulator - k_fixed_step);
        }
    }
}
