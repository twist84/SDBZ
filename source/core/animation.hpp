#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sdbz
{
    struct s_animation_track
    {
        bool m_is_animated = false;
        std::uint16_t m_channel_index = 0;
        std::vector<float> m_times;
        std::vector<std::array<float, 4>> m_values;
    };

    struct s_animation_node
    {
        std::uint16_t m_flags = 0;
        std::string m_name;
        s_animation_track m_translation;
        s_animation_track m_rotation;
        s_animation_track m_scale;
    };

    struct s_animation_clip
    {
        std::string m_name;
        float m_frames_per_second = 0.0f;
        std::uint16_t m_frame_count = 0;
        float m_duration = 0.0f;
        float m_last_frame_time = 0.0f;
        std::array<std::uint16_t, 3> m_channel_counts{};
        std::vector<s_animation_node> m_nodes;
    };

    struct s_motion_package
    {
        std::vector<std::string> m_bone_names;
        std::vector<std::uint8_t> m_bone_flags;
        std::vector<s_animation_clip> m_motions;
    };

    [[nodiscard]] s_animation_clip parse_mot2(
        std::span<const std::uint8_t> data,
        std::string_view name = {});

    [[nodiscard]] s_motion_package parse_rfsm(std::span<const std::uint8_t> data);

    [[nodiscard]] std::array<float, 4> sample_animation_track(
        const s_animation_track& track,
        float time_seconds,
        bool quaternion = false);
}
