#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    struct s_stage_reference
    {
        std::uint32_t m_name_index = 0;
        std::uint32_t m_flags = 0;
        std::string m_name;
    };

    struct s_stage_node
    {
        std::uint32_t m_index = 0;
        std::uint32_t m_source_offset = 0;
        std::uint32_t m_source_size = 0;
        std::uint32_t m_name_index = 0;
        std::uint8_t m_kind = 0;
        std::uint8_t m_links = 0;
        std::uint8_t m_flags = 0;
        std::int32_t m_parent = -1;
        std::string m_name;
        std::array<float, 3> m_translation{};
        std::array<float, 3> m_rotation_radians{};
        std::array<float, 3> m_scale = { 1.0f, 1.0f, 1.0f };
        std::vector<std::uint32_t> m_opaque_words;
        std::vector<s_stage_reference> m_references;
    };

    struct s_stage
    {
        std::uint32_t m_version = 0;
        std::uint32_t m_template_root_count = 0;
        std::vector<std::string> m_names;
        std::vector<std::uint32_t> m_roots;
        std::vector<s_stage_node> m_nodes;
        std::uint32_t m_effect_root = 0;
        std::uint32_t m_stage_root = 0;
    };

    struct s_hit_vertex
    {
        std::array<float, 4> m_position{};
    };

    struct s_hit_polygon
    {
        std::uint32_t m_vertex_count = 0;
        std::uint32_t m_first_vertex = 0;
        std::array<std::uint32_t, 2> m_unknown_words{};
        std::array<float, 4> m_normal{};
    };

    struct s_hit_object
    {
        std::string m_name;
        std::uint32_t m_flags = 0;
        std::uint32_t m_polygon_count = 0;
        std::uint32_t m_first_polygon = 0;
        float m_radius = 0.0f;
        std::array<std::array<float, 4>, 3> m_vectors{};
    };

    struct s_hit
    {
        std::vector<std::string> m_names;
        std::vector<s_hit_object> m_objects;
        std::vector<s_hit_polygon> m_polygons;
        std::vector<s_hit_vertex> m_vertices;
    };

    [[nodiscard]] s_stage parse_stg(std::span<const std::uint8_t> data);
    [[nodiscard]] s_hit parse_hit(std::span<const std::uint8_t> data);
    void write_hit_obj(const std::filesystem::path& path, const s_hit& hit);
}
