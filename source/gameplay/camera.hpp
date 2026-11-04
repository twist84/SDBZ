#pragma once

#include <array>

namespace sdbz
{
    // Shared camera representation. Viewer cameras use positive orthographic
    // m_distance; gameplay cameras use a perspective projection and a signed
    // m_orbit_distance, which may cross through zero to the opposite side.
    struct s_camera
    {
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
        float m_distance = 6.0f;
        std::array<float, 3> m_target{ 0.0f, 0.0f, 0.0f };
        bool m_framed = false;

        bool m_perspective = false;
        float m_orbit_distance = 8.0f;
        float m_vertical_fov_degrees = 55.0f;
        float m_near_plane = 0.05f;
        float m_far_plane = 10000.0f;
        std::array<float, 3> m_follow_offset{ 0.0f, 1.35f, 0.0f };
    };

    struct s_camera_pose
    {
        std::array<float, 3> m_eye{};
        std::array<float, 3> m_target{};
        std::array<float, 3> m_up{ 0.0f, 1.0f, 0.0f };
    };

    void update_follow_camera(s_camera& camera,
        const std::array<float, 3>& followed_position, float delta_seconds);
    void orbit_camera(s_camera& camera, float delta_yaw_degrees,
        float delta_pitch_degrees);
    void zoom_orbit_camera(s_camera& camera, float wheel_steps,
        float distance_per_step = 0.75f);
    [[nodiscard]] s_camera_pose calculate_camera_pose(const s_camera& camera);
}
