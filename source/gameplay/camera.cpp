#include "gameplay/camera.hpp"

#include <algorithm>
#include <cmath>

namespace sdbz
{
    namespace
    {
        constexpr float k_degrees_to_radians = 0.01745329251994329577f;
        constexpr float k_minimum_orbit_distance = 0.15f;

        [[nodiscard]] float sign_nonzero(const float value)
        {
            return value < 0.0f ? -1.0f : 1.0f;
        }
    }

    void update_follow_camera(s_camera& camera,
        const std::array<float, 3>& followed_position, const float delta_seconds)
    {
        const float dt = std::clamp(delta_seconds, 0.0f, 0.1f);
        const float blend = 1.0f - std::exp(-dt * 12.0f);
        for (std::size_t component = 0; component < 3; ++component)
        {
            const float desired = followed_position[component] + camera.m_follow_offset[component];
            camera.m_target[component] += (desired - camera.m_target[component]) * blend;
        }
        camera.m_framed = true;
    }

    void orbit_camera(s_camera& camera, const float delta_yaw_degrees,
        const float delta_pitch_degrees)
    {
        camera.m_yaw += delta_yaw_degrees;
        camera.m_pitch = std::clamp(camera.m_pitch + delta_pitch_degrees, -80.0f, 80.0f);
    }

    void zoom_orbit_camera(s_camera& camera, const float wheel_steps,
        const float distance_per_step)
    {
        if (!std::isfinite(wheel_steps) || !std::isfinite(distance_per_step))
        {
            return;
        }
        const double candidate = static_cast<double>(camera.m_orbit_distance) -
            static_cast<double>(wheel_steps) * static_cast<double>(distance_per_step);
        if (!std::isfinite(candidate))
        {
            return;
        }

        // Keep the signed distance continuous. Clamping it here would prevent
        // high-resolution wheel events from accumulating enough movement to
        // cross zero. The pose calculation applies a small render-only offset
        // around the look-at singularity.
        constexpr double k_maximum_orbit_distance = 1000000.0;
        camera.m_orbit_distance = static_cast<float>(std::clamp(candidate,
            -k_maximum_orbit_distance, k_maximum_orbit_distance));
    }

    s_camera_pose calculate_camera_pose(const s_camera& camera)
    {
        const float yaw = camera.m_yaw * k_degrees_to_radians;
        const float pitch = camera.m_pitch * k_degrees_to_radians;
        float distance = camera.m_orbit_distance;
        if (std::abs(distance) < k_minimum_orbit_distance)
        {
            distance = sign_nonzero(distance) * k_minimum_orbit_distance;
        }
        const float horizontal = std::cos(pitch) * distance;
        const std::array<float, 3> offset{
            std::sin(yaw) * horizontal,
            std::sin(pitch) * distance,
            std::cos(yaw) * horizontal
        };
        return {
            {
                camera.m_target[0] + offset[0],
                camera.m_target[1] + offset[1],
                camera.m_target[2] + offset[2]
            },
            camera.m_target,
            { 0.0f, 1.0f, 0.0f }
        };
    }
}
