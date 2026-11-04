#pragma once

#include "core/math.hpp"
#include "core/stage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdbz
{
    struct s_stage_collision_polygon
    {
        std::vector<std::array<float, 3>> m_vertices;
        std::array<float, 3> m_normal{};
        std::array<float, 3> m_bounds_min{};
        std::array<float, 3> m_bounds_max{};
        float m_area = 0.0f;
        float m_projected_area = 0.0f;
        std::size_t m_stage_node_index = 0;
        std::size_t m_hit_object_index = 0;
        std::size_t m_hit_polygon_index = 0;
        std::string m_object_name;
    };

    struct s_stage_collision_world
    {
        std::vector<s_stage_collision_polygon> m_polygons;
        std::size_t m_unresolved_references = 0;
        std::size_t m_degenerate_polygons = 0;

        // Uniform XZ broad-phase grid. Each polygon is stored in every cell
        // touched by its world-space bounds. Very large polygons are kept in
        // the global list to avoid exploding the grid with arena-sized floors.
        float m_spatial_cell_size = 8.0f;
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_spatial_cells;
        std::vector<std::size_t> m_large_polygon_indices;
        bool m_spatial_index_ready = false;
    };

    struct s_ground_contact
    {
        std::array<float, 3> m_position{};
        std::array<float, 3> m_normal{ 0.0f, 1.0f, 0.0f };
        std::size_t m_collision_polygon_index = 0;
        std::size_t m_stage_node_index = 0;
        std::size_t m_hit_object_index = 0;
        std::string m_object_name;
    };

    struct s_player_spawn
    {
        std::array<float, 3> m_position{};
        std::array<float, 3> m_normal{ 0.0f, 1.0f, 0.0f };
        std::size_t m_collision_polygon_index = 0;
        std::size_t m_stage_node_index = 0;
        std::size_t m_hit_object_index = 0;
        float m_surface_area = 0.0f;
        std::string m_object_name;
    };

    [[nodiscard]] s_stage_collision_world build_stage_collision_world(
        const s_stage& stage, const s_hit& hit, std::span<const mat4> stage_worlds);

    // Rebuild after modifying m_polygons manually. build_stage_collision_world
    // invokes this automatically for parsed STG/HIT resources.
    void build_stage_collision_spatial_index(
        s_stage_collision_world& world, float cell_size = 8.0f);

    [[nodiscard]] std::optional<s_ground_contact> query_stage_ground(
        const s_stage_collision_world& world, float x, float z, float reference_y,
        float max_step_up = 0.35f, float max_drop = 2.0f,
        float max_slope_degrees = 50.0f);

    [[nodiscard]] std::optional<s_player_spawn> find_stage_player_spawn(
        const s_stage_collision_world& world, float foot_offset = 0.05f,
        float max_slope_degrees = 50.0f, float capsule_radius = 0.35f,
        float capsule_height = 1.75f);

    [[nodiscard]] bool is_stage_capsule_clear(
        const s_stage_collision_world& world, float x, float z, float feet_y,
        float capsule_radius = 0.35f, float capsule_height = 1.75f,
        float max_slope_degrees = 50.0f);

    // Verifies that the complete sampled footprint is supported, allowing a
    // capsule to stand across seams between adjacent HIT polygons.
    [[nodiscard]] bool is_stage_capsule_supported(
        const s_stage_collision_world& world, float x, float z, float surface_y,
        float capsule_radius = 0.35f, float max_slope_degrees = 50.0f,
        float surface_tolerance = 0.16f);

    [[nodiscard]] std::array<float, 2> resolve_stage_horizontal_movement(
        const s_stage_collision_world& world, float start_x, float start_z,
        float target_x, float target_z, float feet_y, float capsule_radius = 0.35f,
        float capsule_height = 1.75f, float max_slope_degrees = 50.0f);
}
