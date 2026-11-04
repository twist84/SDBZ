#include "gameplay/player_spawn.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

namespace sdbz
{
    namespace
    {
        [[nodiscard]] std::string normalise_stage_key(std::string key)
        {
            std::replace(key.begin(), key.end(), '\\', '/');
            std::transform(key.begin(), key.end(), key.begin(),
                [](const unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return key;
        }

    }

    std::vector<s_player_spawn_override> parse_player_spawn_overrides(
        const std::string_view text)
    {
        std::vector<s_player_spawn_override> result;
        std::istringstream input{ std::string(text) };
        std::string line;
        while (std::getline(input, line))
        {
            const std::size_t comment = line.find('#');
            if (comment != std::string::npos)
            {
                line.resize(comment);
            }
            std::istringstream row(line);
            s_player_spawn_override entry;
            if (!(row >> entry.m_stage_key))
            {
                continue;
            }
            if (!(row >> entry.m_position[0] >> entry.m_position[1] >>
                entry.m_position[2] >> entry.m_yaw_degrees))
            {
                continue;
            }
            std::string trailing;
            if (row >> trailing || !std::isfinite(entry.m_position[0]) ||
                !std::isfinite(entry.m_position[1]) ||
                !std::isfinite(entry.m_position[2]) ||
                !std::isfinite(entry.m_yaw_degrees))
            {
                continue;
            }
            entry.m_stage_key = normalise_stage_key(std::move(entry.m_stage_key));
            if (!entry.m_stage_key.empty())
            {
                result.push_back(std::move(entry));
            }
        }
        return result;
    }

    std::vector<s_player_spawn_override> load_player_spawn_overrides(
        const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return {};
        }
        const std::string text((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        return parse_player_spawn_overrides(text);
    }

    const s_player_spawn_override* find_player_spawn_override(
        const std::span<const s_player_spawn_override> overrides,
        const std::string_view stage_key)
    {
        const std::string key = normalise_stage_key(std::string(stage_key));
        const auto found = std::find_if(overrides.begin(), overrides.end(),
            [&](const s_player_spawn_override& entry)
            {
                return entry.m_stage_key == key;
            });
        return found == overrides.end() ? nullptr : &*found;
    }

    std::optional<s_player_spawn> resolve_player_spawn_override(
        const s_stage_collision_world& world,
        const s_player_spawn_override& spawn_override,
        const float foot_offset, const float max_slope_degrees,
        const float capsule_radius, const float capsule_height,
        const float vertical_tolerance)
    {
        const float tolerance = std::max(vertical_tolerance, 0.0f);
        const float expected_surface_y = spawn_override.m_position[1] - foot_offset;
        const auto contact = query_stage_ground(world,
            spawn_override.m_position[0], spawn_override.m_position[2],
            expected_surface_y, tolerance, tolerance, max_slope_degrees);
        if (!contact || contact->m_collision_polygon_index >= world.m_polygons.size())
        {
            return std::nullopt;
        }

        const auto& polygon = world.m_polygons[contact->m_collision_polygon_index];
        if (!is_stage_capsule_supported(world,
                spawn_override.m_position[0], spawn_override.m_position[2],
                contact->m_position[1], capsule_radius, max_slope_degrees) ||
            !is_stage_capsule_clear(world,
                spawn_override.m_position[0], spawn_override.m_position[2],
                contact->m_position[1], capsule_radius, capsule_height,
                max_slope_degrees))
        {
            return std::nullopt;
        }

        return s_player_spawn{
            { spawn_override.m_position[0], contact->m_position[1] + foot_offset,
                spawn_override.m_position[2] },
            contact->m_normal, contact->m_collision_polygon_index,
            contact->m_stage_node_index, contact->m_hit_object_index,
            polygon.m_projected_area, contact->m_object_name
        };
    }
}
