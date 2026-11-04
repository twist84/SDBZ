#pragma once

#include "core/stage_collision.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sdbz
{
    struct s_player_spawn_override
    {
        std::string m_stage_key;
        std::array<float, 3> m_position{};
        float m_yaw_degrees = 0.0f;
    };

    [[nodiscard]] std::vector<s_player_spawn_override> parse_player_spawn_overrides(
        std::string_view text);
    [[nodiscard]] std::vector<s_player_spawn_override> load_player_spawn_overrides(
        const std::filesystem::path& path);
    [[nodiscard]] const s_player_spawn_override* find_player_spawn_override(
        std::span<const s_player_spawn_override> overrides, std::string_view stage_key);
    [[nodiscard]] std::optional<s_player_spawn> resolve_player_spawn_override(
        const s_stage_collision_world& world,
        const s_player_spawn_override& spawn_override,
        float foot_offset = 0.05f, float max_slope_degrees = 50.0f,
        float capsule_radius = 0.35f, float capsule_height = 1.75f,
        float vertical_tolerance = 2.0f);
}
