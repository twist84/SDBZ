#include "gameplay/camera.hpp"

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

    bool near(const float a, const float b, const float epsilon = 0.001f)
    {
        return std::abs(a - b) <= epsilon;
    }
}

int main()
{
    sdbz::s_camera viewer_camera;
    sdbz::s_camera player_camera;
    player_camera.m_perspective = true;
    player_camera.m_orbit_distance = 6.0f;
    player_camera.m_follow_offset = { 0.0f, 1.5f, 0.0f };
    player_camera.m_target = { 0.0f, 0.0f, 0.0f };

    sdbz::update_follow_camera(player_camera, { 10.0f, 2.0f, 5.0f }, 1.0f / 60.0f);
    require(player_camera.m_target[0] > 0.0f && player_camera.m_target[2] > 0.0f,
        "gameplay camera follows the player smoothly");
    require(near(viewer_camera.m_target[0], 0.0f) && near(viewer_camera.m_target[2], 0.0f),
        "viewer camera state is independent of gameplay camera follow");

    player_camera.m_target = { 1.0f, 2.0f, 3.0f };
    player_camera.m_yaw = 0.0f;
    player_camera.m_pitch = 0.0f;
    player_camera.m_orbit_distance = 4.0f;
    auto pose = sdbz::calculate_camera_pose(player_camera);
    require(near(pose.m_eye[2], 7.0f), "positive orbit distance places camera in front of target");

    sdbz::zoom_orbit_camera(player_camera, 7.0f, 1.0f);
    require(player_camera.m_orbit_distance < 0.0f,
        "orbit zoom crosses zero and permits negative camera distance");
    pose = sdbz::calculate_camera_pose(player_camera);
    require(pose.m_eye[2] < player_camera.m_target[2],
        "negative orbit distance moves camera to the opposite side of target");
    require(near(pose.m_target[1], player_camera.m_target[1]),
        "signed orbit does not invert or mirror the projection target");

    player_camera.m_orbit_distance = 0.1f;
    sdbz::zoom_orbit_camera(player_camera, 0.1f, 1.0f);
    sdbz::zoom_orbit_camera(player_camera, 0.1f, 1.0f);
    require(player_camera.m_orbit_distance < 0.0f,
        "camera crosses the near-zero singularity after accumulated zoom input");
    player_camera.m_orbit_distance = 0.2f;
    for (int step = 0; step < 4; ++step)
    {
        sdbz::zoom_orbit_camera(player_camera, 0.1f);
    }
    require(player_camera.m_orbit_distance < 0.0f,
        "small high-resolution wheel steps accumulate across zero instead of sticking at the dead zone");
    sdbz::orbit_camera(player_camera, 25.0f, 100.0f);
    require(near(player_camera.m_yaw, 25.0f) && near(player_camera.m_pitch, 80.0f),
        "camera orbit updates yaw and clamps pitch");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " gameplay-camera test(s) failed\n";
        return 1;
    }
    std::cout << "Gameplay-camera tests passed\n";
    return 0;
}
