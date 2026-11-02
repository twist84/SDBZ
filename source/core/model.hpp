#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    struct s_model_node
    {
        std::int32_t m_index = -1;
        std::int32_t m_parent = -1;
        std::int32_t m_first_child = -1;
        std::int32_t m_next_sibling = -1;
        std::string m_name;
        std::array<float, 3> m_scale = { 1.0f, 1.0f, 1.0f };
        std::array<float, 3> m_rotation_degrees = {};
        std::array<float, 3> m_translation = {};
    };

    struct s_model_material
    {
        std::array<float, 4> m_colour_a = {};
        std::array<float, 4> m_colour_b = {};
        std::int32_t m_texture_index = -1;
    };

    struct s_model_vertex
    {
        std::array<float, 3> m_position = {};
        std::array<float, 3> m_normal = {};
        std::array<float, 2> m_uv = {};
        std::array<float, 4> m_colour = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::array<std::uint16_t, 4> m_joints = {};
        std::array<float, 4> m_weights = {};
        std::array<float, 3> m_morph_position = {};
        std::array<float, 3> m_morph_normal = {};
        bool m_has_colour = false;
        bool m_is_skinned = false;
        bool m_has_morph = false;
    };

    struct s_model_primitive
    {
        std::int32_t m_material_index = -1;
        std::vector<std::uint32_t> m_indices;
    };

    struct s_model_mesh
    {
        std::string m_name;
        std::uint32_t m_vertex_flags = 0;
        std::vector<s_model_vertex> m_vertices;
        std::vector<s_model_primitive> m_primitives;
        std::int32_t m_rigid_node_index = -1;
        bool m_is_skinned = false;
        bool m_has_morph = false;
    };

    struct s_model
    {
        std::string m_name;
        std::vector<s_model_node> m_nodes;
        std::vector<s_model_material> m_materials;
        std::vector<s_model_mesh> m_meshes;
    };

    [[nodiscard]] s_model parse_fmdl(std::span<const std::uint8_t> data, const std::string& name = {});
    void write_model_obj(const std::filesystem::path& path, const s_model& model);
}
