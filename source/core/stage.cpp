#include "core/stage.hpp"

#include "core/binary_reader.hpp"

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace sdbz
{
    namespace
    {
        [[nodiscard]] std::uint32_t get_u32(
            const std::span<const std::uint8_t> data, const std::size_t offset, const char* description)
        {
            const auto value = read_u32_le(data, offset);
            if (!value)
            {
                throw std::runtime_error(std::string("truncated ") + description);
            }
            return *value;
        }

        [[nodiscard]] float get_f32(
            const std::span<const std::uint8_t> data, const std::size_t offset, const char* description)
        {
            const auto value = read_f32_le(data, offset);
            if (!value || !std::isfinite(*value))
            {
                throw std::runtime_error(std::string("invalid or non-finite ") + description);
            }
            return *value;
        }

        [[nodiscard]] std::string read_fixed_name(
            const std::span<const std::uint8_t> data, const std::size_t offset)
        {
            if (!has_range(data.size(), offset, 16))
            {
                throw std::runtime_error("truncated fixed-width stage name");
            }

            std::size_t length = 0;
            while (length < 16 && data[offset + length] != 0)
            {
                ++length;
            }
            if (length == 16)
            {
                throw std::runtime_error("unterminated fixed-width stage name");
            }
            return std::string(reinterpret_cast<const char*>(data.data() + offset), length);
        }

        [[nodiscard]] std::uint32_t checked_u32_size(const std::size_t value, const char* description)
        {
            if (value > (std::numeric_limits<std::uint32_t>::max)())
            {
                throw std::runtime_error(std::string(description) + " exceeds 32-bit format limits");
            }
            return static_cast<std::uint32_t>(value);
        }
    }

    s_stage parse_stg(const std::span<const std::uint8_t> data)
    {
        if (data.size() < 16 || get_u32(data, 0, "STG version") != 0x00000100u ||
            fourcc_to_string(data.data() + 4) != "STG ")
        {
            throw std::runtime_error("unsupported STG header or version");
        }

        s_stage result;
        result.m_version = get_u32(data, 0, "STG version");
        const std::uint32_t name_count = get_u32(data, 8, "STG name count");
        result.m_template_root_count = get_u32(data, 12, "STG template root count");
        if (name_count > data.size() / 16u || !has_range(data.size(), 16, static_cast<std::size_t>(name_count) * 16u))
        {
            throw std::runtime_error("STG name table exceeds file bounds");
        }

        result.m_names.reserve(name_count);
        for (std::uint32_t index = 0; index < name_count; ++index)
        {
            result.m_names.push_back(read_fixed_name(data, 16u + static_cast<std::size_t>(index) * 16u));
        }

        std::size_t cursor = 16u + static_cast<std::size_t>(name_count) * 16u;
        std::vector<std::int32_t> pending;
        while (cursor < data.size())
        {
            if (!has_range(data.size(), cursor, 8))
            {
                throw std::runtime_error("truncated STG node header");
            }

            const std::uint8_t kind = data[cursor + 0];
            const std::uint8_t reference_count = data[cursor + 1];
            const std::uint8_t links = data[cursor + 2];
            const std::uint8_t flags = data[cursor + 3];
            const std::uint32_t name_index = get_u32(data, cursor + 4, "STG node name index");
            if (kind < 1 || kind > 6 || links >= 4 || name_index >= result.m_names.size())
            {
                throw std::runtime_error("invalid STG node kind, links, or name index");
            }
            if (reference_count != 0 && kind != 3)
            {
                throw std::runtime_error("STG inline references are only valid on kind 3 nodes");
            }

            std::size_t record_size = 44u + static_cast<std::size_t>(reference_count) * 8u;
            if (kind == 4 || kind == 6)
            {
                record_size = 60;
            }
            else if (kind == 5)
            {
                record_size = 96;
            }
            if (!has_range(data.size(), cursor, record_size))
            {
                throw std::runtime_error("truncated STG node payload");
            }

            s_stage_node node;
            node.m_index = checked_u32_size(result.m_nodes.size(), "STG node index");
            node.m_source_offset = checked_u32_size(cursor, "STG node offset");
            node.m_source_size = checked_u32_size(record_size, "STG node size");
            node.m_name_index = name_index;
            node.m_kind = kind;
            node.m_links = links;
            node.m_flags = flags;
            node.m_name = result.m_names[name_index];
            if (!pending.empty())
            {
                node.m_parent = pending.back();
                pending.pop_back();
            }
            else
            {
                result.m_roots.push_back(node.m_index);
            }

            if (kind <= 3)
            {
                for (std::size_t component = 0; component < 3; ++component)
                {
                    node.m_translation[component] = get_f32(data, cursor + 8 + component * 4, "STG translation");
                    node.m_rotation_radians[component] = get_f32(data, cursor + 20 + component * 4, "STG rotation");
                    node.m_scale[component] = get_f32(data, cursor + 32 + component * 4, "STG scale");
                }
                for (std::size_t reference = 0; reference < reference_count; ++reference)
                {
                    const std::size_t offset = cursor + 44 + reference * 8;
                    const std::uint32_t reference_name = get_u32(data, offset, "STG inline reference name");
                    if (reference_name >= result.m_names.size())
                    {
                        throw std::runtime_error("STG inline reference name index is out of range");
                    }
                    s_stage_reference parsed;
                    parsed.m_name_index = reference_name;
                    parsed.m_flags = get_u32(data, offset + 4, "STG inline reference flags");
                    parsed.m_name = result.m_names[reference_name];
                    node.m_references.push_back(std::move(parsed));
                }
            }
            else
            {
                for (std::size_t offset = cursor + 8; offset < cursor + record_size; offset += 4)
                {
                    node.m_opaque_words.push_back(get_u32(data, offset, "STG opaque control word"));
                }
            }

            result.m_nodes.push_back(std::move(node));
            const std::int32_t parent_index = result.m_nodes.back().m_parent;
            if ((links & 2u) != 0)
            {
                pending.push_back(parent_index);
            }
            if ((links & 1u) != 0)
            {
                pending.push_back(static_cast<std::int32_t>(result.m_nodes.back().m_index));
            }
            cursor += record_size;
        }

        if (!pending.empty())
        {
            throw std::runtime_error("STG hierarchy ends before promised child or sibling");
        }
        if (result.m_roots.size() != static_cast<std::size_t>(result.m_template_root_count) + 2u)
        {
            throw std::runtime_error("STG root count does not match template count plus EFFECT/STAGE roots");
        }
        result.m_effect_root = result.m_roots[result.m_roots.size() - 2];
        result.m_stage_root = result.m_roots.back();
        if (result.m_nodes[result.m_effect_root].m_name != "EFFECT" ||
            result.m_nodes[result.m_stage_root].m_name != "STAGE")
        {
            throw std::runtime_error("STG final roots are not named EFFECT and STAGE");
        }

        return result;
    }

    s_hit parse_hit(const std::span<const std::uint8_t> data)
    {
        if (data.size() < 32 || get_u32(data, 0, "HIT version") != 0x00020000u ||
            fourcc_to_string(data.data() + 4) != "HIT ")
        {
            throw std::runtime_error("unsupported HIT header or version");
        }

        const std::uint32_t object_count = get_u32(data, 8, "HIT object count");
        const std::uint32_t objects_offset = get_u32(data, 12, "HIT objects offset");
        const std::uint32_t polygons_offset = get_u32(data, 16, "HIT polygons offset");
        const std::uint32_t vertices_offset = get_u32(data, 20, "HIT vertices offset");
        const std::uint64_t expected_objects = 32ull + static_cast<std::uint64_t>(object_count) * 16ull;
        const std::uint64_t expected_polygons = expected_objects + static_cast<std::uint64_t>(object_count) * 64ull;
        if (objects_offset != expected_objects || polygons_offset != expected_polygons ||
            vertices_offset < polygons_offset || vertices_offset > data.size() ||
            (vertices_offset - polygons_offset) % 32u != 0 || (data.size() - vertices_offset) % 16u != 0)
        {
            throw std::runtime_error("invalid HIT object, polygon, or vertex extents");
        }
        if (!has_range(data.size(), 32, static_cast<std::size_t>(object_count) * 16u) ||
            !has_range(data.size(), objects_offset, static_cast<std::size_t>(object_count) * 64u))
        {
            throw std::runtime_error("HIT object/name table exceeds file bounds");
        }

        s_hit result;
        result.m_names.reserve(object_count);
        result.m_objects.reserve(object_count);
        for (std::uint32_t index = 0; index < object_count; ++index)
        {
            result.m_names.push_back(read_fixed_name(data, 32u + static_cast<std::size_t>(index) * 16u));
        }

        const std::size_t vertex_count = (data.size() - vertices_offset) / 16u;
        result.m_vertices.reserve(vertex_count);
        for (std::size_t index = 0; index < vertex_count; ++index)
        {
            s_hit_vertex vertex;
            for (std::size_t component = 0; component < 4; ++component)
            {
                vertex.m_position[component] = get_f32(data, vertices_offset + index * 16u + component * 4u, "HIT vertex");
            }
            result.m_vertices.push_back(vertex);
        }

        const std::size_t polygon_count = (vertices_offset - polygons_offset) / 32u;
        result.m_polygons.reserve(polygon_count);
        std::uint64_t expected_first_vertex = 0;
        for (std::size_t index = 0; index < polygon_count; ++index)
        {
            const std::size_t offset = polygons_offset + index * 32u;
            s_hit_polygon polygon;
            polygon.m_vertex_count = get_u32(data, offset, "HIT polygon vertex count");
            polygon.m_first_vertex = get_u32(data, offset + 4, "HIT polygon first vertex");
            polygon.m_unknown_words[0] = get_u32(data, offset + 8, "HIT polygon unknown word");
            polygon.m_unknown_words[1] = get_u32(data, offset + 12, "HIT polygon unknown word");
            for (std::size_t component = 0; component < 4; ++component)
            {
                polygon.m_normal[component] = get_f32(data, offset + 16 + component * 4u, "HIT polygon normal");
            }
            if (polygon.m_vertex_count < 3 || polygon.m_first_vertex != expected_first_vertex ||
                static_cast<std::uint64_t>(polygon.m_first_vertex) + polygon.m_vertex_count > vertex_count)
            {
                throw std::runtime_error("invalid HIT polygon vertex range");
            }
            expected_first_vertex += polygon.m_vertex_count;
            result.m_polygons.push_back(polygon);
        }
        if (expected_first_vertex != vertex_count)
        {
            throw std::runtime_error("HIT contains an unreferenced vertex tail");
        }

        for (std::uint32_t index = 0; index < object_count; ++index)
        {
            const std::size_t offset = objects_offset + static_cast<std::size_t>(index) * 64u;
            s_hit_object object;
            object.m_name = result.m_names[index];
            object.m_flags = get_u32(data, offset, "HIT object flags");
            object.m_polygon_count = get_u32(data, offset + 4, "HIT object polygon count");
            object.m_first_polygon = get_u32(data, offset + 8, "HIT object first polygon");
            object.m_radius = get_f32(data, offset + 12, "HIT object radius");
            if (static_cast<std::uint64_t>(object.m_first_polygon) + object.m_polygon_count > polygon_count)
            {
                throw std::runtime_error("HIT object polygon range exceeds polygon table");
            }
            for (std::size_t vector = 0; vector < 3; ++vector)
            {
                for (std::size_t component = 0; component < 4; ++component)
                {
                    object.m_vectors[vector][component] = get_f32(
                        data, offset + 16 + vector * 16u + component * 4u, "HIT object vector");
                }
            }
            result.m_objects.push_back(std::move(object));
        }
        return result;
    }

    void write_hit_obj(const std::filesystem::path& path, const s_hit& hit)
    {
        std::ostringstream output;
        output << "# Resource-local HIT polygons; STG placement is not applied.\n";
        for (const auto& vertex : hit.m_vertices)
        {
            output << "v " << vertex.m_position[0] << ' ' << vertex.m_position[1] << ' '
                   << vertex.m_position[2] << '\n';
        }
        for (const auto& object : hit.m_objects)
        {
            output << "o ";
            for (const char character : object.m_name)
            {
                output << ((character == ' ' || character == '\\' || character == '/') ? '_' : character);
            }
            output << '\n';
            const std::uint64_t end = static_cast<std::uint64_t>(object.m_first_polygon) + object.m_polygon_count;
            for (std::uint64_t polygon_index = object.m_first_polygon; polygon_index < end; ++polygon_index)
            {
                const auto& polygon = hit.m_polygons[static_cast<std::size_t>(polygon_index)];
                output << 'f';
                for (std::uint32_t vertex = 0; vertex < polygon.m_vertex_count; ++vertex)
                {
                    output << ' ' << (static_cast<std::uint64_t>(polygon.m_first_vertex) + vertex + 1u);
                }
                output << '\n';
            }
        }
        const std::string text = output.str();
        write_file(path, std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
    }
}
