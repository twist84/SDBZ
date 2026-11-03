#include "core/model.hpp"

#include "core/binary_reader.hpp"
#include "core/container.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace sdbz
{
    namespace
    {
        struct s_chunk_view
        {
            std::string m_magic;
            std::span<const std::uint8_t> m_data;
            std::uint32_t m_offset = 0;
        };

        [[nodiscard]] std::vector<s_chunk_view> get_children(
            const std::span<const std::uint8_t> data,
            const std::string& expected_magic)
        {
            const auto directory = parse_chunked_container(data, expected_magic);
            std::vector<s_chunk_view> result;
            result.reserve(directory.m_chunks.size());
            for (const auto& chunk : directory.m_chunks)
            {
                result.push_back(s_chunk_view
                {
                    chunk.magic_string(),
                    data.subspan(chunk.m_offset, chunk.m_size),
                    chunk.m_offset
                });
            }
            return result;
        }

        [[nodiscard]] const s_chunk_view* find_chunk(
            const std::vector<s_chunk_view>& chunks,
            const std::string_view magic)
        {
            const auto it = std::find_if(chunks.begin(), chunks.end(), [magic](const s_chunk_view& chunk)
            {
                return chunk.m_magic == magic;
            });
            return it == chunks.end() ? nullptr : &*it;
        }

        [[nodiscard]] std::array<float, 4> read_vec4(
            const std::span<const std::uint8_t> data,
            const std::size_t offset)
        {
            std::array<float, 4> result{};
            for (std::size_t component = 0; component < result.size(); ++component)
            {
                const auto value = read_f32_le(data, offset + component * 4);
                if (!value || !std::isfinite(*value))
                {
                    throw std::runtime_error("truncated or non-finite FMDL float vector");
                }
                result[component] = *value;
            }
            return result;
        }

        [[nodiscard]] std::vector<std::string> read_string_list(
            const std::span<const std::uint8_t> data,
            const std::size_t count)
        {
            if (data.size() < 32)
            {
                throw std::runtime_error("truncated FMDL name-list chunk");
            }

            std::vector<std::string> result;
            result.reserve(count);
            std::size_t offset = 32;
            for (std::size_t index = 0; index < count; ++index)
            {
                if (offset >= data.size())
                {
                    throw std::runtime_error("FMDL name list is truncated");
                }
                const auto begin = data.begin() + static_cast<std::ptrdiff_t>(offset);
                const auto end = std::find(begin, data.end(), 0);
                result.emplace_back(begin, end);
                offset += static_cast<std::size_t>(std::distance(begin, end)) + 1;
            }
            return result;
        }

        [[nodiscard]] std::vector<s_model_node> parse_hierarchy(
            const std::vector<s_chunk_view>& top_chunks)
        {
            const auto* dhie = find_chunk(top_chunks, "DHIE");
            if (!dhie)
            {
                return {};
            }

            const auto hierarchy_chunks = get_children(dhie->m_data, "DHIE");
            const auto* hierarchy = find_chunk(hierarchy_chunks, "XHIE");
            if (!hierarchy)
            {
                throw std::runtime_error("DHIE does not contain XHIE");
            }

            const auto count_value = read_u32_le(hierarchy->m_data, 8);
            if (!count_value)
            {
                throw std::runtime_error("XHIE is missing its node count");
            }
            const std::size_t count = *count_value;
            if (count > ((std::numeric_limits<std::size_t>::max)() - 32) / 64 ||
                hierarchy->m_data.size() != 32 + count * 64)
            {
                throw std::runtime_error("unexpected XHIE record size");
            }

            std::vector<std::string> names;
            if (const auto* xnon = find_chunk(top_chunks, "XNON"))
            {
                names = read_string_list(xnon->m_data, count);
            }

            std::vector<s_model_node> nodes;
            nodes.reserve(count);
            for (std::size_t index = 0; index < count; ++index)
            {
                const std::size_t offset = 32 + index * 64;
                const auto node_index = read_i32_le(hierarchy->m_data, offset);
                const auto parent = read_i32_le(hierarchy->m_data, offset + 4);
                const auto first_child = read_i32_le(hierarchy->m_data, offset + 8);
                const auto next_sibling = read_i32_le(hierarchy->m_data, offset + 12);
                if (!node_index || !parent || !first_child || !next_sibling)
                {
                    throw std::runtime_error("truncated XHIE node record");
                }

                const auto valid_link = [count](const std::int32_t link)
                {
                    return link == -1 || (link >= 0 && static_cast<std::size_t>(link) < count);
                };
                if (*node_index != static_cast<std::int32_t>(index) ||
                    !valid_link(*parent) || !valid_link(*first_child) || !valid_link(*next_sibling))
                {
                    throw std::runtime_error("invalid XHIE node index or hierarchy link");
                }

                const auto scale = read_vec4(hierarchy->m_data, offset + 16);
                const auto rotation = read_vec4(hierarchy->m_data, offset + 32);
                const auto translation = read_vec4(hierarchy->m_data, offset + 48);
                s_model_node node;
                node.m_index = *node_index;
                node.m_parent = *parent;
                node.m_first_child = *first_child;
                node.m_next_sibling = *next_sibling;
                node.m_name = index < names.size() ? names[index] : "node_" + std::to_string(index);
                node.m_scale = { scale[0], scale[1], scale[2] };
                node.m_rotation_degrees = { rotation[0], rotation[1], rotation[2] };
                node.m_translation = { translation[0], translation[1], translation[2] };
                nodes.push_back(std::move(node));
            }

            // Detect parent cycles before any later world-transform traversal.
            std::vector<std::uint8_t> visit_state(nodes.size(), 0);
            const auto visit_parent = [&nodes, &visit_state](const auto& self, const std::size_t index) -> void
            {
                if (visit_state[index] == 1)
                {
                    throw std::runtime_error("FMDL skeleton contains a parent cycle");
                }
                if (visit_state[index] == 2)
                {
                    return;
                }
                visit_state[index] = 1;
                const auto parent_index = nodes[index].m_parent;
                if (parent_index >= 0)
                {
                    self(self, static_cast<std::size_t>(parent_index));
                }
                visit_state[index] = 2;
            };
            for (std::size_t index = 0; index < nodes.size(); ++index)
            {
                visit_parent(visit_parent, index);
            }

            return nodes;
        }

        [[nodiscard]] std::vector<s_model_material> parse_materials(
            const std::vector<s_chunk_view>& top_chunks,
            std::vector<std::vector<std::int32_t>>& assignments,
            const std::size_t mesh_count)
        {
            const auto* dmat = find_chunk(top_chunks, "DMAT");
            if (!dmat)
            {
                throw std::runtime_error("FMDL is missing its DMAT material directory");
            }

            const auto material_chunks = get_children(dmat->m_data, "DMAT");
            const auto* xmat = find_chunk(material_chunks, "XMAT");
            const auto* xmal = find_chunk(material_chunks, "XMAL");
            if (!xmat || !xmal)
            {
                throw std::runtime_error("DMAT must contain XMAT and XMAL");
            }

            const auto material_count_value = read_u32_le(xmat->m_data, 8);
            if (!material_count_value)
            {
                throw std::runtime_error("XMAT is missing its material count");
            }
            const std::size_t material_count = *material_count_value;
            if (xmat->m_data.size() != 32 + material_count * 48)
            {
                throw std::runtime_error("unexpected XMAT material record size");
            }
            if (xmal->m_data.size() != 32 + mesh_count * 128)
            {
                throw std::runtime_error("unexpected XMAL per-mesh assignment size");
            }

            std::vector<s_model_material> materials;
            materials.reserve(material_count);
            for (std::size_t index = 0; index < material_count; ++index)
            {
                const std::size_t offset = 32 + index * 48;
                const auto colour_a = read_vec4(xmat->m_data, offset);
                const auto colour_b = read_vec4(xmat->m_data, offset + 16);
                const auto texture_index = read_i32_le(xmat->m_data, offset + 32);
                if (!texture_index)
                {
                    throw std::runtime_error("truncated XMAT material record");
                }
                materials.push_back({ colour_a, colour_b, *texture_index });
            }

            assignments.clear();
            assignments.resize(mesh_count);
            for (std::size_t mesh = 0; mesh < mesh_count; ++mesh)
            {
                auto& row = assignments[mesh];
                row.reserve(32);
                for (std::size_t slot = 0; slot < 32; ++slot)
                {
                    const auto material_index = read_i32_le(xmal->m_data, 32 + mesh * 128 + slot * 4);
                    if (!material_index)
                    {
                        throw std::runtime_error("truncated XMAL assignment row");
                    }
                    if (*material_index >= 0 && static_cast<std::size_t>(*material_index) >= material_count)
                    {
                        throw std::runtime_error("XMAL references a material outside XMAT");
                    }
                    row.push_back(*material_index);
                }
            }

            return materials;
        }

        [[nodiscard]] s_model_mesh parse_xmod(
            const std::span<const std::uint8_t> data,
            const std::vector<std::int32_t>& palette,
            const std::vector<std::int32_t>& material_indices,
            const std::size_t bone_count)
        {
            if (data.size() < 0x1B0 || fourcc_to_string(data.data()) != "XMOD")
            {
                throw std::runtime_error("invalid or truncated XMOD mesh");
            }

            const auto flags_value = read_u32_le(data, 12);
            const auto expected_vertex_count = read_u32_le(data, 48);
            const auto expected_triangle_count = read_u32_le(data, 52);
            if (!flags_value || !expected_vertex_count || !expected_triangle_count)
            {
                throw std::runtime_error("XMOD header is truncated");
            }
            const std::uint32_t flags = *flags_value;
            if (flags != 0 && flags != 1 && flags != 2 && flags != 3 && flags != 4 && flags != 6 && flags != 11)
            {
                throw std::runtime_error("unsupported XMOD vertex flags: " + std::to_string(flags));
            }

            const bool skinned = (flags & 2u) != 0 && (flags & 8u) == 0;
            const bool morph = (flags & 8u) != 0;
            const bool coloured = (flags & 4u) != 0;
            const std::size_t stride = 3u + ((flags & 2u) != 0 ? 2u : 0u) + (coloured ? 1u : 0u);

            s_model_mesh mesh;
            mesh.m_vertex_flags = flags;
            mesh.m_is_skinned = skinned;
            mesh.m_has_morph = morph;
            mesh.m_rigid_node_index = skinned ? -1 : palette[0];
            mesh.m_primitives.reserve(material_indices.size());
            for (const auto material_index : material_indices)
            {
                mesh.m_primitives.push_back({ material_index, {} });
            }

            std::size_t dma_offset = 0x1A0;
            std::size_t material_cursor = 0;
            while (dma_offset < data.size())
            {
                const auto dma_tag = read_u32_le(data, dma_offset);
                const auto dma_reserved = read_u32_le(data, dma_offset + 4);
                if (!dma_tag || !dma_reserved || (*dma_tag >> 28) != 6 || *dma_reserved != 0)
                {
                    throw std::runtime_error("unsupported XMOD DMA tag");
                }

                const std::uint64_t dma_end64 = static_cast<std::uint64_t>(dma_offset) + 16ull +
                    static_cast<std::uint64_t>(*dma_tag & 0xFFFFu) * 16ull;
                if (dma_end64 > data.size() || material_cursor >= mesh.m_primitives.size())
                {
                    throw std::runtime_error("XMOD DMA/material extent is invalid");
                }
                const std::size_t dma_end = static_cast<std::size_t>(dma_end64);
                std::size_t offset = dma_offset + 16;
                auto& primitive = mesh.m_primitives[material_cursor];

                while (offset < dma_end)
                {
                    const auto vif_word = read_u32_le(data, offset);
                    if (!vif_word)
                    {
                        throw std::runtime_error("truncated XMOD VIF command");
                    }
                    const std::uint32_t command = (*vif_word >> 24) & 0x7Fu;
                    offset += 4;
                    if (command == 0 || command == 1 || command == 0x17)
                    {
                        continue;
                    }
                    if (command != 0x6C)
                    {
                        throw std::runtime_error("unsupported XMOD VIF command: " + std::to_string(command));
                    }

                    std::size_t vector_count = (*vif_word >> 16) & 0xFFu;
                    if (vector_count == 0)
                    {
                        vector_count = 256;
                    }
                    if (vector_count < 2 || vector_count > (dma_end - offset) / 16)
                    {
                        throw std::runtime_error("XMOD VIF UNPACK extent is invalid");
                    }

                    const auto gif_tag = read_u32_le(data, offset + 16);
                    if (!gif_tag)
                    {
                        throw std::runtime_error("XMOD VIF packet is missing its GIF tag");
                    }
                    const std::size_t vertex_count = *gif_tag & 0x7FFFu;
                    if (vertex_count == 0 || vector_count != 2 + vertex_count * stride)
                    {
                        throw std::runtime_error("unexpected XMOD VIF vertex-packet layout");
                    }
                    const std::size_t payload_end = offset + vector_count * 16;
                    if (payload_end > dma_end)
                    {
                        throw std::runtime_error("XMOD VIF packet exceeds its DMA segment");
                    }

                    for (std::size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index)
                    {
                        const std::size_t vertex_offset = offset + 32 + vertex_index * stride * 16;
                        std::vector<std::array<float, 4>> vectors;
                        vectors.reserve(stride);
                        for (std::size_t vector_index = 0; vector_index < stride; ++vector_index)
                        {
                            vectors.push_back(read_vec4(data, vertex_offset + vector_index * 16));
                        }

                        const auto adc_word = read_u32_le(data, vertex_offset + 12);
                        if (!adc_word)
                        {
                            throw std::runtime_error("XMOD vertex position is truncated");
                        }

                        s_model_vertex vertex;
                        vertex.m_position = { vectors[0][0], vectors[0][1], vectors[0][2] };
                        vertex.m_normal = { vectors[1][0], vectors[1][1], vectors[1][2] };
                        const std::size_t uv_vector = coloured ? 3 : 2;
                        vertex.m_uv = { vectors[uv_vector][0], vectors[uv_vector][1] };
                        vertex.m_has_colour = coloured;
                        if (coloured)
                        {
                            for (std::size_t component = 0; component < 4; ++component)
                            {
                                vertex.m_colour[component] = vectors[2][component] / 128.0f;
                            }
                        }

                        if (skinned)
                        {
                            const std::size_t joint_vector = coloured ? 4 : 3;
                            const std::size_t joint_offset = vertex_offset + joint_vector * 16;
                            const auto weights = vectors.back();
                            float weight_sum = 0.0f;
                            for (std::size_t influence = 0; influence < 4; ++influence)
                            {
                                const auto raw_address = read_u32_le(data, joint_offset + influence * 4);
                                if (!raw_address)
                                {
                                    throw std::runtime_error("truncated XMOD skin palette addresses");
                                }
                                const float weight = weights[influence];
                                if (!std::isfinite(weight))
                                {
                                    throw std::runtime_error("non-finite XMOD skin weight");
                                }
                                weight_sum += weight;

                                std::int32_t joint = 0;
                                if (weight != 0.0f)
                                {
                                    if ((*raw_address & 3u) != 0 || (*raw_address / 4u) >= palette.size())
                                    {
                                        throw std::runtime_error("invalid XMOD skin palette address");
                                    }
                                    joint = palette[*raw_address / 4u];
                                    if (joint < 0 || static_cast<std::size_t>(joint) >= bone_count || joint > 0xFFFF)
                                    {
                                        throw std::runtime_error("XMOD skin references a missing hierarchy node");
                                    }
                                }
                                vertex.m_joints[influence] = static_cast<std::uint16_t>(joint);
                                vertex.m_weights[influence] = weight;
                            }
                            if (std::abs(weight_sum - 1.0f) >= 1.0e-4f)
                            {
                                throw std::runtime_error("XMOD skin weights do not sum to one");
                            }
                            vertex.m_is_skinned = true;
                        }

                        if (morph)
                        {
                            const auto& morph_position = vectors[vectors.size() - 2];
                            const auto& morph_normal = vectors[vectors.size() - 1];
                            vertex.m_morph_position = { morph_position[0], morph_position[1], morph_position[2] };
                            vertex.m_morph_normal = { morph_normal[0], morph_normal[1], morph_normal[2] };
                            vertex.m_has_morph = true;
                        }

                        mesh.m_vertices.push_back(vertex);
                        if ((*adc_word & 0x8000u) == 0)
                        {
                            if (mesh.m_vertices.size() < 3)
                            {
                                throw std::runtime_error("XMOD triangle strip starts before three vertices");
                            }
                            const std::uint32_t vertex_end = static_cast<std::uint32_t>(mesh.m_vertices.size() - 1);
                            const std::uint32_t orientation_index = vertex_end - 2;
                            const bool reverse = vectors[1][3] < 0.0f;
                            if (reverse)
                            {
                                primitive.m_indices.push_back(vertex_end - 1);
                                primitive.m_indices.push_back(orientation_index);
                            }
                            else
                            {
                                primitive.m_indices.push_back(orientation_index);
                                primitive.m_indices.push_back(vertex_end - 1);
                            }
                            primitive.m_indices.push_back(vertex_end);
                        }
                    }

                    offset = payload_end;
                }

                dma_offset = dma_end;
                ++material_cursor;
            }

            if (material_cursor != mesh.m_primitives.size())
            {
                throw std::runtime_error("XMOD DMA count does not match its assigned materials");
            }
            if (mesh.m_vertices.size() != *expected_vertex_count)
            {
                throw std::runtime_error("XMOD decoded vertex count does not match its header");
            }

            std::uint64_t triangle_count = 0;
            for (const auto& primitive : mesh.m_primitives)
            {
                if ((primitive.m_indices.size() % 3) != 0)
                {
                    throw std::runtime_error("XMOD primitive has an incomplete triangle");
                }
                triangle_count += primitive.m_indices.size() / 3;
            }
            if (triangle_count != *expected_triangle_count)
            {
                throw std::runtime_error("XMOD decoded triangle count does not match its header");
            }

            return mesh;
        }

        struct s_matrix4
        {
            std::array<float, 16> m_value =
            {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            };
        };

        [[nodiscard]] s_matrix4 multiply(const s_matrix4& left, const s_matrix4& right)
        {
            s_matrix4 result{};
            result.m_value.fill(0.0f);
            for (std::size_t row = 0; row < 4; ++row)
            {
                for (std::size_t column = 0; column < 4; ++column)
                {
                    for (std::size_t inner = 0; inner < 4; ++inner)
                    {
                        result.m_value[row * 4 + column] +=
                            left.m_value[row * 4 + inner] * right.m_value[inner * 4 + column];
                    }
                }
            }
            return result;
        }

        [[nodiscard]] s_matrix4 local_transform(const s_model_node& node)
        {
            constexpr float k_degrees_to_radians = 0.01745329251994329577f;
            const float x = node.m_rotation_degrees[0] * k_degrees_to_radians;
            const float y = node.m_rotation_degrees[1] * k_degrees_to_radians;
            const float z = node.m_rotation_degrees[2] * k_degrees_to_radians;
            const float cx = std::cos(x);
            const float sx = std::sin(x);
            const float cy = std::cos(y);
            const float sy = std::sin(y);
            const float cz = std::cos(z);
            const float sz = std::sin(z);

            s_matrix4 scale{};
            scale.m_value[0] = node.m_scale[0];
            scale.m_value[5] = node.m_scale[1];
            scale.m_value[10] = node.m_scale[2];

            s_matrix4 rotate_x{};
            rotate_x.m_value[5] = cx;
            rotate_x.m_value[6] = -sx;
            rotate_x.m_value[9] = sx;
            rotate_x.m_value[10] = cx;

            s_matrix4 rotate_y{};
            rotate_y.m_value[0] = cy;
            rotate_y.m_value[2] = sy;
            rotate_y.m_value[8] = -sy;
            rotate_y.m_value[10] = cy;

            s_matrix4 rotate_z{};
            rotate_z.m_value[0] = cz;
            rotate_z.m_value[1] = -sz;
            rotate_z.m_value[4] = sz;
            rotate_z.m_value[5] = cz;

            s_matrix4 translation{};
            translation.m_value[3] = node.m_translation[0];
            translation.m_value[7] = node.m_translation[1];
            translation.m_value[11] = node.m_translation[2];

            return multiply(translation,
                multiply(rotate_z, multiply(rotate_y, multiply(rotate_x, scale))));
        }

        [[nodiscard]] std::vector<s_matrix4> build_world_transforms(const std::vector<s_model_node>& nodes)
        {
            std::vector<s_matrix4> world(nodes.size());
            std::vector<std::uint8_t> visit_state(nodes.size(), 0);
            const auto build = [&nodes, &world, &visit_state](const auto& self, const std::size_t index) -> const s_matrix4&
            {
                if (visit_state[index] == 1)
                {
                    throw std::runtime_error("cannot export a model with a hierarchy cycle");
                }
                if (visit_state[index] == 2)
                {
                    return world[index];
                }
                visit_state[index] = 1;
                const auto local = local_transform(nodes[index]);
                if (nodes[index].m_parent >= 0)
                {
                    if (static_cast<std::size_t>(nodes[index].m_parent) >= nodes.size())
                    {
                        throw std::runtime_error("cannot export a model with an invalid parent node");
                    }
                    world[index] = multiply(self(self, static_cast<std::size_t>(nodes[index].m_parent)), local);
                }
                else if (nodes[index].m_parent != -1)
                {
                    throw std::runtime_error("cannot export a model with an invalid parent node");
                }
                else
                {
                    world[index] = local;
                }
                visit_state[index] = 2;
                return world[index];
            };

            for (std::size_t index = 0; index < nodes.size(); ++index)
            {
                (void)build(build, index);
            }
            return world;
        }

        [[nodiscard]] std::array<float, 3> transform_position(
            const s_matrix4& matrix,
            const std::array<float, 3>& position)
        {
            return
            {
                matrix.m_value[0] * position[0] + matrix.m_value[1] * position[1] +
                    matrix.m_value[2] * position[2] + matrix.m_value[3],
                matrix.m_value[4] * position[0] + matrix.m_value[5] * position[1] +
                    matrix.m_value[6] * position[2] + matrix.m_value[7],
                matrix.m_value[8] * position[0] + matrix.m_value[9] * position[1] +
                    matrix.m_value[10] * position[2] + matrix.m_value[11]
            };
        }

        [[nodiscard]] std::array<float, 3> transform_normal(
            const s_matrix4& matrix,
            const std::array<float, 3>& normal)
        {
            const float a00 = matrix.m_value[0];
            const float a01 = matrix.m_value[1];
            const float a02 = matrix.m_value[2];
            const float a10 = matrix.m_value[4];
            const float a11 = matrix.m_value[5];
            const float a12 = matrix.m_value[6];
            const float a20 = matrix.m_value[8];
            const float a21 = matrix.m_value[9];
            const float a22 = matrix.m_value[10];

            const float c00 = a11 * a22 - a12 * a21;
            const float c01 = a12 * a20 - a10 * a22;
            const float c02 = a10 * a21 - a11 * a20;
            const float c10 = a02 * a21 - a01 * a22;
            const float c11 = a00 * a22 - a02 * a20;
            const float c12 = a01 * a20 - a00 * a21;
            const float c20 = a01 * a12 - a02 * a11;
            const float c21 = a02 * a10 - a00 * a12;
            const float c22 = a00 * a11 - a01 * a10;
            const float determinant = a00 * c00 + a01 * c01 + a02 * c02;

            std::array<float, 3> transformed{};
            if (std::abs(determinant) > 1.0e-8f)
            {
                const float inverse_determinant = 1.0f / determinant;
                transformed =
                {
                    (c00 * normal[0] + c01 * normal[1] + c02 * normal[2]) * inverse_determinant,
                    (c10 * normal[0] + c11 * normal[1] + c12 * normal[2]) * inverse_determinant,
                    (c20 * normal[0] + c21 * normal[1] + c22 * normal[2]) * inverse_determinant
                };
            }
            else
            {
                transformed =
                {
                    a00 * normal[0] + a01 * normal[1] + a02 * normal[2],
                    a10 * normal[0] + a11 * normal[1] + a12 * normal[2],
                    a20 * normal[0] + a21 * normal[1] + a22 * normal[2]
                };
            }

            const float length = std::sqrt(
                transformed[0] * transformed[0] +
                transformed[1] * transformed[1] +
                transformed[2] * transformed[2]);
            if (length > 1.0e-8f)
            {
                for (float& component : transformed)
                {
                    component /= length;
                }
            }
            return transformed;
        }

        [[nodiscard]] std::string obj_group_name(const std::string& value, const std::size_t index)
        {
            std::string result;
            result.reserve(value.size());
            for (const char raw_ch : value)
            {
                const auto ch = static_cast<unsigned char>(raw_ch);
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
                {
                    result.push_back(static_cast<char>(ch));
                }
                else if (ch >= 0x80)
                {
                    result.push_back('_');
                }
            }
            if (result.empty())
            {
                result = "mesh_" + std::to_string(index);
            }
            return result;
        }
    }

    s_model parse_fmdl(const std::span<const std::uint8_t> data, std::string_view name)
    {
        const auto top_directory = parse_chunked_container(data, "FMDL");
        (void)top_directory;
        const auto top_chunks = get_children(data, "FMDL");

        s_model model;
        model.m_name = name;
        model.m_nodes = parse_hierarchy(top_chunks);

        const auto* dmod = find_chunk(top_chunks, "DMOD");
        const auto* xmon = find_chunk(top_chunks, "XMON");
        if (!dmod)
        {
            throw std::runtime_error("FMDL is missing its DMOD mesh directory");
        }

        const auto mesh_chunks = get_children(dmod->m_data, "DMOD");
        std::vector<const s_chunk_view*> xmods;
        for (const auto& chunk : mesh_chunks)
        {
            if (chunk.m_magic == "XMOD")
            {
                xmods.push_back(&chunk);
            }
        }
        if (xmods.empty())
        {
            throw std::runtime_error("DMOD does not contain any XMOD meshes");
        }

        std::vector<std::string> mesh_names;
        if (xmon)
        {
            mesh_names = read_string_list(xmon->m_data, xmods.size());
        }

        std::vector<std::vector<std::int32_t>> material_assignments;
        model.m_materials = parse_materials(top_chunks, material_assignments, xmods.size());

        std::vector<std::vector<std::int32_t>> palettes(xmods.size(), std::vector<std::int32_t>(32, -1));
        std::size_t palette_index = 0;
        for (const auto& chunk : top_chunks)
        {
            if (chunk.m_magic != "XMTX")
            {
                continue;
            }
            if (chunk.m_data.size() < 32 + 128)
            {
                throw std::runtime_error("XMTX palette count or size does not match DMOD meshes");
            }
            const std::size_t palette_count = (chunk.m_data.size() - 32) / (32 * 4);
            for (std::size_t palette = 0; palette < palette_count; ++palette)
            {
                if (palette_index >= xmods.size())
                {
                    break;
                }
                for (std::size_t slot = 0; slot < 32; ++slot)
                {
                    const auto node_index = read_i32_le(chunk.m_data, 32 + palette * 32 * 4 + slot * 4);
                    if (!node_index)
                    {
                        throw std::runtime_error("truncated XMTX palette row");
                    }
                    if (*node_index < -1 || (*node_index >= 0 && static_cast<std::size_t>(*node_index) >= model.m_nodes.size()))
                    {
                        throw std::runtime_error("XMTX palette references an invalid hierarchy node");
                    }
                    palettes[palette_index][slot] = *node_index;
                }
                ++palette_index;
            }
        }
        if (palette_index != 0 && palette_index != xmods.size())
        {
            throw std::runtime_error("partial XMTX palette table");
        }

        model.m_meshes.reserve(xmods.size());
        for (std::size_t index = 0; index < xmods.size(); ++index)
        {
            std::vector<std::int32_t> assigned;
            for (const auto material_index : material_assignments[index])
            {
                if (material_index >= 0)
                {
                    assigned.push_back(material_index);
                }
            }
            if (assigned.empty())
            {
                throw std::runtime_error("XMOD mesh has no material assignments");
            }

            auto mesh = parse_xmod(xmods[index]->m_data, palettes[index], assigned, model.m_nodes.size());
            mesh.m_name = index < mesh_names.size() ? mesh_names[index] : "mesh_" + std::to_string(index);
            model.m_meshes.push_back(std::move(mesh));
        }

        return model;
    }

    void write_model_obj(const std::filesystem::path& path, const s_model& model)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("could not create OBJ output: " + path.string());
        }
        output << "# Static FMDL bind-pose export\n"
               << "# Skinned vertices are retained in authored model bind space; no animation is applied.\n"
               << std::setprecision(9);

        const auto world = build_world_transforms(model.m_nodes);
        std::uint64_t base_index = 1;
        for (std::size_t mesh_index = 0; mesh_index < model.m_meshes.size(); ++mesh_index)
        {
            const auto& mesh = model.m_meshes[mesh_index];
            const s_matrix4* transform = nullptr;
            if (!mesh.m_is_skinned && mesh.m_rigid_node_index >= 0)
            {
                if (static_cast<std::size_t>(mesh.m_rigid_node_index) >= world.size())
                {
                    throw std::runtime_error("OBJ export mesh references an invalid rigid node");
                }
                transform = &world[static_cast<std::size_t>(mesh.m_rigid_node_index)];
            }

            output << "\ng " << obj_group_name(mesh.m_name, mesh_index) << '\n';
            for (const auto& vertex : mesh.m_vertices)
            {
                const auto position = transform ? transform_position(*transform, vertex.m_position) : vertex.m_position;
                output << "v " << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
            }
            for (const auto& vertex : mesh.m_vertices)
            {
                output << "vt " << vertex.m_uv[0] << ' ' << (1.0f - vertex.m_uv[1]) << '\n';
            }
            for (const auto& vertex : mesh.m_vertices)
            {
                const auto normal = transform ? transform_normal(*transform, vertex.m_normal) : vertex.m_normal;
                output << "vn " << normal[0] << ' ' << normal[1] << ' ' << normal[2] << '\n';
            }

            for (const auto& primitive : mesh.m_primitives)
            {
                output << "usemtl material_" << primitive.m_material_index << '\n';
                for (std::size_t index = 0; index + 2 < primitive.m_indices.size(); index += 3)
                {
                    const std::uint64_t a = base_index + primitive.m_indices[index];
                    const std::uint64_t b = base_index + primitive.m_indices[index + 1];
                    const std::uint64_t c = base_index + primitive.m_indices[index + 2];
                    if (a >= base_index + mesh.m_vertices.size() ||
                        b >= base_index + mesh.m_vertices.size() ||
                        c >= base_index + mesh.m_vertices.size())
                    {
                        throw std::runtime_error("OBJ export encountered an invalid mesh index");
                    }
                    output << "f " << a << '/' << a << '/' << a << ' '
                           << b << '/' << b << '/' << b << ' '
                           << c << '/' << c << '/' << c << '\n';
                }
            }
            base_index += mesh.m_vertices.size();
        }

        if (!output)
        {
            throw std::runtime_error("failed while writing OBJ output: " + path.string());
        }
    }
}

