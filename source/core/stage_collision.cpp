#include "core/stage_collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace sdbz
{
    namespace
    {
        constexpr float k_epsilon = 1.0e-5f;
        constexpr float k_degrees_to_radians = 0.01745329251994329577f;
        constexpr std::uint64_t k_max_polygon_grid_cells = 4096;
        constexpr std::uint64_t k_max_query_grid_cells = 8192;

        [[nodiscard]] float dot3(
            const std::array<float, 3>& a, const std::array<float, 3>& b)
        {
            return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        }

        [[nodiscard]] float length3(const std::array<float, 3>& value)
        {
            return std::sqrt(dot3(value, value));
        }

        [[nodiscard]] std::array<float, 3> polygon_normal(
            const std::vector<std::array<float, 3>>& vertices)
        {
            // Newell's method works for arbitrary planar n-gons and does not
            // assume that the first three points are non-collinear.
            std::array<float, 3> normal{};
            for (std::size_t index = 0; index < vertices.size(); ++index)
            {
                const auto& current = vertices[index];
                const auto& next = vertices[(index + 1) % vertices.size()];
                normal[0] += (current[1] - next[1]) * (current[2] + next[2]);
                normal[1] += (current[2] - next[2]) * (current[0] + next[0]);
                normal[2] += (current[0] - next[0]) * (current[1] + next[1]);
            }
            const float length = length3(normal);
            if (length <= k_epsilon)
            {
                return {};
            }
            for (float& component : normal)
            {
                component /= length;
            }
            // Collision queries describe support surfaces, so use an upward
            // normal independent of the source polygon winding.
            if (normal[1] < 0.0f)
            {
                for (float& component : normal)
                {
                    component = -component;
                }
            }
            return normal;
        }

        [[nodiscard]] float projected_area_xz(
            const std::vector<std::array<float, 3>>& vertices)
        {
            float twice_area = 0.0f;
            for (std::size_t index = 0; index < vertices.size(); ++index)
            {
                const auto& current = vertices[index];
                const auto& next = vertices[(index + 1) % vertices.size()];
                twice_area += current[0] * next[2] - next[0] * current[2];
            }
            return std::abs(twice_area) * 0.5f;
        }

        [[nodiscard]] bool point_in_polygon_xz(
            const std::vector<std::array<float, 3>>& vertices, float x, float z)
        {
            bool inside = false;
            for (std::size_t index = 0, previous = vertices.size() - 1;
                index < vertices.size(); previous = index++)
            {
                const float ax = vertices[previous][0];
                const float az = vertices[previous][2];
                const float bx = vertices[index][0];
                const float bz = vertices[index][2];
                const float edge_x = bx - ax;
                const float edge_z = bz - az;
                const float point_x = x - ax;
                const float point_z = z - az;
                const float edge_length_sq = edge_x * edge_x + edge_z * edge_z;
                if (edge_length_sq > k_epsilon)
                {
                    const float t = std::clamp(
                        (point_x * edge_x + point_z * edge_z) / edge_length_sq,
                        0.0f, 1.0f);
                    const float dx = point_x - edge_x * t;
                    const float dz = point_z - edge_z * t;
                    if (dx * dx + dz * dz <= k_epsilon * k_epsilon)
                    {
                        return true;
                    }
                }
                const bool crosses = ((az > z) != (bz > z)) &&
                    (x < (bx - ax) * (z - az) / (bz - az) + ax);
                if (crosses)
                {
                    inside = !inside;
                }
            }
            return inside;
        }

        [[nodiscard]] float distance_to_polygon_edge_xz(
            const std::vector<std::array<float, 3>>& vertices, float x, float z)
        {
            float minimum_distance_sq = (std::numeric_limits<float>::max)();
            for (std::size_t index = 0; index < vertices.size(); ++index)
            {
                const auto& a = vertices[index];
                const auto& b = vertices[(index + 1) % vertices.size()];
                const float dx = b[0] - a[0];
                const float dz = b[2] - a[2];
                const float length_sq = dx * dx + dz * dz;
                const float t = length_sq > k_epsilon
                    ? std::clamp(((x - a[0]) * dx + (z - a[2]) * dz) / length_sq, 0.0f, 1.0f)
                    : 0.0f;
                const float px = a[0] + dx * t;
                const float pz = a[2] + dz * t;
                const float ex = x - px;
                const float ez = z - pz;
                minimum_distance_sq = std::min(minimum_distance_sq, ex * ex + ez * ez);
            }
            return std::sqrt(minimum_distance_sq);
        }

        [[nodiscard]] bool is_walkable(
            const s_stage_collision_polygon& polygon, const float minimum_normal_y)
        {
            return polygon.m_normal[1] >= minimum_normal_y &&
                polygon.m_projected_area > k_epsilon;
        }

        [[nodiscard]] bool to_cell_coordinate(
            const float value, const float cell_size, std::int32_t& result)
        {
            if (!std::isfinite(value) || !std::isfinite(cell_size) || cell_size <= k_epsilon)
            {
                return false;
            }
            const double cell = std::floor(
                static_cast<double>(value) / static_cast<double>(cell_size));
            if (cell < static_cast<double>((std::numeric_limits<std::int32_t>::lowest)()) ||
                cell > static_cast<double>((std::numeric_limits<std::int32_t>::max)()))
            {
                return false;
            }
            result = static_cast<std::int32_t>(cell);
            return true;
        }

        [[nodiscard]] std::uint64_t make_cell_key(
            const std::int32_t cell_x, const std::int32_t cell_z)
        {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x)) << 32) |
                static_cast<std::uint32_t>(cell_z);
        }

        void append_all_polygon_indices(
            const s_stage_collision_world& world, std::vector<std::size_t>& result)
        {
            result.resize(world.m_polygons.size());
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                result[index] = index;
            }
        }

        [[nodiscard]] std::vector<std::size_t> collect_collision_candidates(
            const s_stage_collision_world& world,
            const float minimum_x, const float minimum_z,
            const float maximum_x, const float maximum_z)
        {
            std::vector<std::size_t> result;
            if (!world.m_spatial_index_ready)
            {
                append_all_polygon_indices(world, result);
                return result;
            }

            std::int32_t minimum_cell_x = 0;
            std::int32_t minimum_cell_z = 0;
            std::int32_t maximum_cell_x = 0;
            std::int32_t maximum_cell_z = 0;
            if (!to_cell_coordinate(minimum_x, world.m_spatial_cell_size, minimum_cell_x) ||
                !to_cell_coordinate(minimum_z, world.m_spatial_cell_size, minimum_cell_z) ||
                !to_cell_coordinate(maximum_x, world.m_spatial_cell_size, maximum_cell_x) ||
                !to_cell_coordinate(maximum_z, world.m_spatial_cell_size, maximum_cell_z) ||
                maximum_cell_x < minimum_cell_x || maximum_cell_z < minimum_cell_z)
            {
                append_all_polygon_indices(world, result);
                return result;
            }

            const std::uint64_t cell_width = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(maximum_cell_x) - minimum_cell_x + 1);
            const std::uint64_t cell_depth = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(maximum_cell_z) - minimum_cell_z + 1);
            if (cell_width > k_max_query_grid_cells ||
                cell_depth > k_max_query_grid_cells ||
                cell_width * cell_depth > k_max_query_grid_cells)
            {
                append_all_polygon_indices(world, result);
                return result;
            }

            result.insert(result.end(), world.m_large_polygon_indices.begin(),
                world.m_large_polygon_indices.end());
            for (std::int64_t cell_x = minimum_cell_x;
                cell_x <= maximum_cell_x; ++cell_x)
            {
                for (std::int64_t cell_z = minimum_cell_z;
                    cell_z <= maximum_cell_z; ++cell_z)
                {
                    const auto cell = world.m_spatial_cells.find(make_cell_key(
                        static_cast<std::int32_t>(cell_x),
                        static_cast<std::int32_t>(cell_z)));
                    if (cell != world.m_spatial_cells.end())
                    {
                        result.insert(result.end(), cell->second.begin(), cell->second.end());
                    }
                }
            }
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());
            return result;
        }

        [[nodiscard]] bool height_on_polygon(
            const s_stage_collision_polygon& polygon, float x, float z, float& out_y)
        {
            const float normal_y = polygon.m_normal[1];
            if (std::abs(normal_y) <= k_epsilon ||
                x < polygon.m_bounds_min[0] - k_epsilon ||
                x > polygon.m_bounds_max[0] + k_epsilon ||
                z < polygon.m_bounds_min[2] - k_epsilon ||
                z > polygon.m_bounds_max[2] + k_epsilon ||
                !point_in_polygon_xz(polygon.m_vertices, x, z))
            {
                return false;
            }
            const auto& origin = polygon.m_vertices.front();
            out_y = origin[1] - (
                polygon.m_normal[0] * (x - origin[0]) +
                polygon.m_normal[2] * (z - origin[2])) / normal_y;
            return std::isfinite(out_y);
        }

        [[nodiscard]] std::optional<std::array<float, 2>> find_polygon_interior_point(
            const s_stage_collision_polygon& polygon)
        {
            // The polygon area centroid is an excellent candidate for the
            // usual convex arena floor. Verify it, then fall back to a bounded
            // grid search so concave authored surfaces remain supported.
            float twice_area = 0.0f;
            float centroid_x = 0.0f;
            float centroid_z = 0.0f;
            for (std::size_t index = 0; index < polygon.m_vertices.size(); ++index)
            {
                const auto& a = polygon.m_vertices[index];
                const auto& b = polygon.m_vertices[(index + 1) % polygon.m_vertices.size()];
                const float cross = a[0] * b[2] - b[0] * a[2];
                twice_area += cross;
                centroid_x += (a[0] + b[0]) * cross;
                centroid_z += (a[2] + b[2]) * cross;
            }
            if (std::abs(twice_area) > k_epsilon)
            {
                centroid_x /= 3.0f * twice_area;
                centroid_z /= 3.0f * twice_area;
                if (point_in_polygon_xz(polygon.m_vertices, centroid_x, centroid_z))
                {
                    return std::array<float, 2>{ centroid_x, centroid_z };
                }
            }

            float best_x = 0.0f;
            float best_z = 0.0f;
            float best_clearance = -1.0f;
            constexpr int k_grid_steps = 16;
            for (int z_index = 1; z_index < k_grid_steps; ++z_index)
            {
                const float z_fraction = static_cast<float>(z_index) /
                    static_cast<float>(k_grid_steps);
                const float z = polygon.m_bounds_min[2] +
                    (polygon.m_bounds_max[2] - polygon.m_bounds_min[2]) * z_fraction;
                for (int x_index = 1; x_index < k_grid_steps; ++x_index)
                {
                    const float x_fraction = static_cast<float>(x_index) /
                        static_cast<float>(k_grid_steps);
                    const float x = polygon.m_bounds_min[0] +
                        (polygon.m_bounds_max[0] - polygon.m_bounds_min[0]) * x_fraction;
                    if (!point_in_polygon_xz(polygon.m_vertices, x, z))
                    {
                        continue;
                    }
                    const float clearance = distance_to_polygon_edge_xz(
                        polygon.m_vertices, x, z);
                    if (clearance > best_clearance)
                    {
                        best_clearance = clearance;
                        best_x = x;
                        best_z = z;
                    }
                }
            }
            if (best_clearance < 0.0f)
            {
                return std::nullopt;
            }
            return std::array<float, 2>{ best_x, best_z };
        }

        [[nodiscard]] bool capsule_clear_impl(
            const s_stage_collision_world& world, const float x, const float z,
            const float feet_y, const float capsule_radius, const float capsule_height,
            const float minimum_normal_y)
        {
            const float radius = std::max(capsule_radius, 0.0f);
            const float height = std::max(capsule_height, 0.0f);

            // Cross-section radius of a vertical capsule at height y. If the
            // requested height is shorter than its diameter, treat it as a
            // sphere whose diameter is the available height.
            const float cap_radius = std::min(radius, height * 0.5f);
            const float cylinder_bottom = feet_y + cap_radius;
            const float cylinder_top = feet_y + height - cap_radius;
            auto radius_at_y = [&](float y) -> float
            {
                if (height <= 2.0f * cap_radius + k_epsilon)
                {
                    const float dy = y - (feet_y + height * 0.5f);
                    return std::sqrt(std::max(0.0f, cap_radius * cap_radius - dy * dy));
                }
                if (y < cylinder_bottom)
                {
                    const float dy = y - cylinder_bottom;
                    return std::sqrt(std::max(0.0f, cap_radius * cap_radius - dy * dy));
                }
                if (y > cylinder_top)
                {
                    const float dy = y - cylinder_top;
                    return std::sqrt(std::max(0.0f, cap_radius * cap_radius - dy * dy));
                }
                return cap_radius;
            };

            const auto candidates = collect_collision_candidates(
                world, x - radius, z - radius, x + radius, z + radius);
            for (const std::size_t polygon_index : candidates)
            {
                const auto& polygon = world.m_polygons[polygon_index];
                if (polygon.m_bounds_max[0] < x - radius - k_epsilon ||
                    polygon.m_bounds_min[0] > x + radius + k_epsilon ||
                    polygon.m_bounds_max[2] < z - radius - k_epsilon ||
                    polygon.m_bounds_min[2] > z + radius + k_epsilon)
                {
                    continue;
                }
                const float overlap_min_y = std::max(feet_y, polygon.m_bounds_min[1]);
                const float overlap_max_y = std::min(feet_y + height, polygon.m_bounds_max[1]);
                if (overlap_min_y > overlap_max_y + k_epsilon)
                {
                    continue;
                }

                // Walkable polygons can also be overhead platforms or ceilings.
                // Ignore the support surface at the player's feet, but reject
                // any other walkable face intersecting the capsule volume.
                if (polygon.m_normal[1] >= minimum_normal_y)
                {
                    float surface_y = 0.0f;
                    if (height_on_polygon(polygon, x, z, surface_y) &&
                        surface_y > feet_y + 0.08f && surface_y < feet_y + height - 0.05f)
                    {
                        return false;
                    }
                    continue;
                }

                // Near-vertical HIT polygons are blocking walls. Approximate
                // the capsule with a vertical cylinder for this first runtime
                // pass, testing both the plane distance and its finite tangent
                // extent.
                const float normal_length = std::sqrt(
                    polygon.m_normal[0] * polygon.m_normal[0] +
                    polygon.m_normal[2] * polygon.m_normal[2]);
                if (normal_length <= k_epsilon)
                {
                    continue;
                }
                const float nx = polygon.m_normal[0] / normal_length;
                const float nz = polygon.m_normal[2] / normal_length;
                const auto& origin = polygon.m_vertices.front();
                const float distance = std::abs(
                    (x - origin[0]) * nx + (z - origin[2]) * nz);

                // Use the largest capsule cross-section within the wall's
                // vertical bounds. height_on_polygon() is not suitable here:
                // a vertical HIT wall has no single height at a given XZ.
                const float wall_y = std::clamp(feet_y + height * 0.5f,
                    overlap_min_y, overlap_max_y);
                const float effective_radius = radius_at_y(wall_y);
                if (distance > effective_radius)
                {
                    continue;
                }
                const float tangent_x = -nz;
                const float tangent_z = nx;
                const float tangent = x * tangent_x + z * tangent_z;
                float tangent_min = (std::numeric_limits<float>::max)();
                float tangent_max = (std::numeric_limits<float>::lowest)();
                for (const auto& vertex : polygon.m_vertices)
                {
                    const float value = vertex[0] * tangent_x + vertex[2] * tangent_z;
                    tangent_min = std::min(tangent_min, value);
                    tangent_max = std::max(tangent_max, value);
                }
                if (tangent >= tangent_min - effective_radius &&
                    tangent <= tangent_max + effective_radius)
                {
                    return false;
                }
            }
            return true;
        }
    }

    void build_stage_collision_spatial_index(
        s_stage_collision_world& world, const float cell_size)
    {
        world.m_spatial_cells.clear();
        world.m_large_polygon_indices.clear();
        world.m_spatial_cell_size = std::isfinite(cell_size) && cell_size > k_epsilon
            ? cell_size : 8.0f;
        world.m_spatial_index_ready = false;

        for (std::size_t polygon_index = 0;
            polygon_index < world.m_polygons.size(); ++polygon_index)
        {
            const auto& polygon = world.m_polygons[polygon_index];
            std::int32_t minimum_cell_x = 0;
            std::int32_t minimum_cell_z = 0;
            std::int32_t maximum_cell_x = 0;
            std::int32_t maximum_cell_z = 0;
            if (!to_cell_coordinate(polygon.m_bounds_min[0], world.m_spatial_cell_size,
                    minimum_cell_x) ||
                !to_cell_coordinate(polygon.m_bounds_min[2], world.m_spatial_cell_size,
                    minimum_cell_z) ||
                !to_cell_coordinate(polygon.m_bounds_max[0], world.m_spatial_cell_size,
                    maximum_cell_x) ||
                !to_cell_coordinate(polygon.m_bounds_max[2], world.m_spatial_cell_size,
                    maximum_cell_z) ||
                maximum_cell_x < minimum_cell_x || maximum_cell_z < minimum_cell_z)
            {
                world.m_large_polygon_indices.push_back(polygon_index);
                continue;
            }

            const std::uint64_t cell_width = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(maximum_cell_x) - minimum_cell_x + 1);
            const std::uint64_t cell_depth = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(maximum_cell_z) - minimum_cell_z + 1);
            if (cell_width > k_max_polygon_grid_cells ||
                cell_depth > k_max_polygon_grid_cells ||
                cell_width * cell_depth > k_max_polygon_grid_cells)
            {
                world.m_large_polygon_indices.push_back(polygon_index);
                continue;
            }

            for (std::int64_t cell_x = minimum_cell_x;
                cell_x <= maximum_cell_x; ++cell_x)
            {
                for (std::int64_t cell_z = minimum_cell_z;
                    cell_z <= maximum_cell_z; ++cell_z)
                {
                    world.m_spatial_cells[make_cell_key(
                        static_cast<std::int32_t>(cell_x),
                        static_cast<std::int32_t>(cell_z))].push_back(polygon_index);
                }
            }
        }
        world.m_spatial_index_ready = true;
    }

    s_stage_collision_world build_stage_collision_world(
        const s_stage& stage, const s_hit& hit, const std::span<const mat4> stage_worlds)
    {
        s_stage_collision_world result;
        std::unordered_map<std::string, std::size_t> hit_objects_by_name;
        hit_objects_by_name.reserve(hit.m_objects.size());
        for (std::size_t index = 0; index < hit.m_objects.size(); ++index)
        {
            hit_objects_by_name.emplace(hit.m_objects[index].m_name, index);
        }

        // An STG holds template (prototype) roots, an EFFECT root and the live
        // STAGE root. Only HIT nodes under STAGE are part of the playable
        // world: template/effect subtrees are stored near the origin and are
        // never placed in the arena, so letting them into the collision world
        // made the player spawn on a hidden surface beneath the stage.
        // Parents precede children in m_nodes, so one forward pass suffices.
        // Synthetic stages without parsed roots keep every node.
        const bool has_root_info = !stage.m_roots.empty() &&
            stage.m_stage_root < stage.m_nodes.size();
        if (has_root_info && stage.m_stage_root < stage_worlds.size())
        {
            result.m_stage_origin_y = mat4_point(
                stage_worlds[stage.m_stage_root], { 0.0f, 0.0f, 0.0f })[1];
        }
        std::vector<char> in_stage_root(stage.m_nodes.size(), 0);
        if (has_root_info)
        {
            for (std::size_t i = 0; i < stage.m_nodes.size(); ++i)
            {
                const auto& n = stage.m_nodes[i];
                if (n.m_index == stage.m_stage_root)
                {
                    in_stage_root[i] = 1;
                }
                else if (n.m_parent >= 0 &&
                    static_cast<std::size_t>(n.m_parent) < i)
                {
                    in_stage_root[i] = in_stage_root[static_cast<std::size_t>(n.m_parent)];
                }
            }
        }

        for (std::size_t node_slot = 0; node_slot < stage.m_nodes.size(); ++node_slot)
        {
            const auto& node = stage.m_nodes[node_slot];
            if (node.m_kind != 3 || node.m_references.empty() ||
                node.m_index >= stage_worlds.size() ||
                (has_root_info && !in_stage_root[node_slot]))
            {
                continue;
            }
            const mat4& world = stage_worlds[node.m_index];
            for (const auto& reference : node.m_references)
            {
                const auto object_it = hit_objects_by_name.find(reference.m_name);
                if (object_it == hit_objects_by_name.end())
                {
                    ++result.m_unresolved_references;
                    continue;
                }
                const std::size_t object_index = object_it->second;
                const auto& object = hit.m_objects[object_index];
                const std::uint64_t polygon_end =
                    static_cast<std::uint64_t>(object.m_first_polygon) + object.m_polygon_count;
                for (std::uint64_t polygon_index = object.m_first_polygon;
                    polygon_index < polygon_end; ++polygon_index)
                {
                    if (polygon_index >= hit.m_polygons.size())
                    {
                        break;
                    }
                    const auto& source_polygon = hit.m_polygons[
                        static_cast<std::size_t>(polygon_index)];
                    s_stage_collision_polygon polygon;
                    polygon.m_stage_node_index = node.m_index;
                    polygon.m_hit_object_index = object_index;
                    polygon.m_hit_polygon_index = static_cast<std::size_t>(polygon_index);
                    polygon.m_object_name = object.m_name;
                    polygon.m_vertices.reserve(source_polygon.m_vertex_count);
                    polygon.m_bounds_min = {
                        (std::numeric_limits<float>::max)(),
                        (std::numeric_limits<float>::max)(),
                        (std::numeric_limits<float>::max)()
                    };
                    polygon.m_bounds_max = {
                        (std::numeric_limits<float>::lowest)(),
                        (std::numeric_limits<float>::lowest)(),
                        (std::numeric_limits<float>::lowest)()
                    };

                    for (std::uint32_t vertex_index = 0;
                        vertex_index < source_polygon.m_vertex_count; ++vertex_index)
                    {
                        const std::uint64_t source_index =
                            static_cast<std::uint64_t>(source_polygon.m_first_vertex) + vertex_index;
                        if (source_index >= hit.m_vertices.size())
                        {
                            polygon.m_vertices.clear();
                            break;
                        }
                        const auto& source_position = hit.m_vertices[
                            static_cast<std::size_t>(source_index)].m_position;
                        const auto transformed = mat4_point(world, {
                            source_position[0], source_position[1], source_position[2] });
                        polygon.m_vertices.push_back(transformed);
                        for (std::size_t component = 0; component < 3; ++component)
                        {
                            polygon.m_bounds_min[component] = std::min(
                                polygon.m_bounds_min[component], transformed[component]);
                            polygon.m_bounds_max[component] = std::max(
                                polygon.m_bounds_max[component], transformed[component]);
                        }
                    }
                    if (polygon.m_vertices.size() < 3)
                    {
                        ++result.m_degenerate_polygons;
                        continue;
                    }

                    polygon.m_normal = polygon_normal(polygon.m_vertices);
                    if (length3(polygon.m_normal) <= k_epsilon)
                    {
                        ++result.m_degenerate_polygons;
                        continue;
                    }
                    polygon.m_projected_area = projected_area_xz(polygon.m_vertices);
                    // Store the full 3D area. The Newell vector magnitude is
                    // twice the polygon area before normalisation.
                    std::array<float, 3> newell{};
                    for (std::size_t index = 0; index < polygon.m_vertices.size(); ++index)
                    {
                        const auto& current = polygon.m_vertices[index];
                        const auto& next = polygon.m_vertices[(index + 1) % polygon.m_vertices.size()];
                        newell[0] += (current[1] - next[1]) * (current[2] + next[2]);
                        newell[1] += (current[2] - next[2]) * (current[0] + next[0]);
                        newell[2] += (current[0] - next[0]) * (current[1] + next[1]);
                    }
                    polygon.m_area = length3(newell) * 0.5f;
                    result.m_polygons.push_back(std::move(polygon));
                }
            }
        }
        build_stage_collision_spatial_index(result);
        return result;
    }

    std::optional<s_ground_contact> query_stage_ground(
        const s_stage_collision_world& world, const float x, const float z,
        const float reference_y, const float max_step_up, const float max_drop,
        const float max_slope_degrees)
    {
        const float minimum_normal_y = std::cos(
            std::clamp(max_slope_degrees, 0.0f, 89.0f) * k_degrees_to_radians);
        std::optional<s_ground_contact> result;
        float best_height = (std::numeric_limits<float>::lowest)();
        const auto candidates = collect_collision_candidates(world, x, z, x, z);
        for (const std::size_t index : candidates)
        {
            const auto& polygon = world.m_polygons[index];
            if (!is_walkable(polygon, minimum_normal_y))
            {
                continue;
            }
            float height = 0.0f;
            if (!height_on_polygon(polygon, x, z, height) ||
                height > reference_y + std::max(max_step_up, 0.0f) ||
                height < reference_y - std::max(max_drop, 0.0f) ||
                height <= best_height)
            {
                continue;
            }
            best_height = height;
            result = s_ground_contact{
                { x, height, z }, polygon.m_normal, index,
                polygon.m_stage_node_index, polygon.m_hit_object_index,
                polygon.m_object_name
            };
        }
        return result;
    }

    [[nodiscard]] std::optional<s_ground_contact> query_stage_ground_capsule(
        const s_stage_collision_world& world, const float x, const float z,
        const float reference_y, const float capsule_radius,
        const float max_step_up, const float max_drop,
        const float max_slope_degrees)
    {
        // A vertical capsule's lowest point is at its centreline. Requiring
        // every point around its projected radius to overlap a floor polygon
        // is equivalent to treating the capsule as a flat-bottomed cylinder;
        // this drops contact on narrow HIT strips, triangle seams and small
        // gaps even while the actual bottom contact point is over the floor.
        // Use the exact centre query as the reference point; the footprint
        // check below remains authoritative for wide floors near their edges.
        const auto centre = query_stage_ground(world, x, z, reference_y,
            max_step_up, max_drop, max_slope_degrees);
        const float radius = std::max(capsule_radius, 0.0f);
        if (centre)
        {
            if (is_stage_capsule_supported(world, x, z, centre->m_position[1],
                    radius, max_slope_degrees))
            {
                return centre;
            }

            // A narrow, elongated HIT surface can legitimately support a
            // capsule at its centreline even though a flat disk of radius R
            // would overhang both sides. This is common for authored strips
            // and rails. Do not apply this exception to a wide floor near its
            // edge: there the footprint check still causes an edge fall.
            if (centre->m_collision_polygon_index < world.m_polygons.size() &&
                radius > k_epsilon)
            {
                const auto& polygon = world.m_polygons[centre->m_collision_polygon_index];
                const float extent_x = polygon.m_bounds_max[0] - polygon.m_bounds_min[0];
                const float extent_z = polygon.m_bounds_max[2] - polygon.m_bounds_min[2];
                const float narrow_extent = std::min(extent_x, extent_z);
                const float centre_clearance = distance_to_polygon_edge_xz(
                    polygon.m_vertices, x, z);
                if (narrow_extent <= radius * 2.0f + 0.01f &&
                    centre_clearance >= radius * 0.45f)
                {
                    return centre;
                }
            }
            return std::nullopt;
        }

        // HIT floors can contain tiny cracks between independently authored
        // polygons. If the centre falls exactly into one, reconstruct its
        // support height from a majority of nearby samples on the same plane.
        // This is only a seam fallback: a single polygon at the edge cannot
        // keep the player grounded once the centre has left its support.
        if (radius <= k_epsilon)
        {
            return std::nullopt;
        }

        struct s_nearby_support
        {
            s_ground_contact m_contact;
            float m_centre_y = 0.0f;
        };
        std::vector<s_nearby_support> supports;
        constexpr int k_ring_samples = 16;
        constexpr float k_two_pi = 6.28318530717958647692f;
        const float sample_radius = radius * 0.85f;
        const float expanded_step_up = std::max(max_step_up, 0.0f) + radius;
        const float expanded_drop = std::max(max_drop, 0.0f) + radius;
        supports.reserve(k_ring_samples);

        for (int sample_index = 0; sample_index < k_ring_samples; ++sample_index)
        {
            const float angle = k_two_pi * static_cast<float>(sample_index) /
                static_cast<float>(k_ring_samples);
            const float sample_x = x + std::cos(angle) * sample_radius;
            const float sample_z = z + std::sin(angle) * sample_radius;
            const auto contact = query_stage_ground(world, sample_x, sample_z,
                reference_y, expanded_step_up, expanded_drop, max_slope_degrees);
            if (!contact || contact->m_normal[1] <= k_epsilon)
            {
                continue;
            }

            const float centre_y = contact->m_position[1] +
                (contact->m_normal[0] * (sample_x - x) +
                    contact->m_normal[2] * (sample_z - z)) / contact->m_normal[1];
            if (!std::isfinite(centre_y) ||
                centre_y > reference_y + expanded_step_up ||
                centre_y < reference_y - expanded_drop)
            {
                continue;
            }
            supports.push_back({ *contact, centre_y });
        }

        // A majority is required so a nearby wall top or isolated adjacent
        // platform cannot manufacture a ground contact across an open gap.
        if (supports.size() < static_cast<std::size_t>(k_ring_samples / 2))
        {
            return std::nullopt;
        }
        std::sort(supports.begin(), supports.end(),
            [](const s_nearby_support& a, const s_nearby_support& b)
            {
                return a.m_centre_y < b.m_centre_y;
            });
        const auto& median = supports[supports.size() / 2];
        const float height_tolerance = std::max(0.12f, radius * 0.5f);
        std::size_t matching_count = 0;
        for (const auto& support : supports)
        {
            if (std::abs(support.m_centre_y - median.m_centre_y) <= height_tolerance)
            {
                ++matching_count;
            }
        }
        if (matching_count < static_cast<std::size_t>(k_ring_samples / 2))
        {
            return std::nullopt;
        }

        s_ground_contact result = median.m_contact;
        result.m_position = { x, median.m_centre_y, z };
        return result;
    }

    std::optional<s_player_spawn> find_stage_player_spawn(
        const s_stage_collision_world& world, const float foot_offset,
        const float max_slope_degrees, const float capsule_radius,
        const float capsule_height)
    {
        const float minimum_normal_y = std::cos(
            std::clamp(max_slope_degrees, 0.0f, 89.0f) * k_degrees_to_radians);

        // Spawn candidates are ranked against the live STAGE root origin, not
        // absolute world Y=0. The STAGE root may itself be translated in the
        // world. Prefer the nearest valid surface height first, then deep
        // interior clearance and the largest playable footprint. HIT object names
        // are retained for diagnostics but are not assigned guessed semantics.
        struct s_rank
        {
            float m_height_delta = 0.0f;
            float m_clearance = 0.0f;
            float m_area = 0.0f;
        };
        const auto is_better = [](const s_rank& a, const s_rank& b)
        {
            if (a.m_height_delta != b.m_height_delta)
            {
                return a.m_height_delta < b.m_height_delta;
            }
            if (a.m_clearance != b.m_clearance)
            {
                return a.m_clearance > b.m_clearance;
            }
            return a.m_area > b.m_area;
        };

        struct s_point
        {
            float m_x = 0.0f;
            float m_z = 0.0f;
            float m_y = 0.0f;
            float m_clearance = 0.0f;
            float m_height_delta = 0.0f;
        };

        std::optional<s_player_spawn> result;
        s_rank best_rank;
        constexpr int k_max_points_per_polygon = 48;
        const float reference_y = world.m_stage_origin_y;

        for (std::size_t index = 0; index < world.m_polygons.size(); ++index)
        {
            const auto& polygon = world.m_polygons[index];
            if (!is_walkable(polygon, minimum_normal_y) || polygon.m_vertices.size() < 3)
            {
                continue;
            }

            // The polygon's vertical bounds provide a safe lower bound on its
            // possible distance from the stage origin. This avoids running
            // capsule tests on surfaces that cannot beat the current result.
            float minimum_height_delta = 0.0f;
            if (reference_y < polygon.m_bounds_min[1])
            {
                minimum_height_delta = polygon.m_bounds_min[1] - reference_y;
            }
            else if (reference_y > polygon.m_bounds_max[1])
            {
                minimum_height_delta = reference_y - polygon.m_bounds_max[1];
            }
            if (result && minimum_height_delta > best_rank.m_height_delta + 0.001f)
            {
                continue;
            }

            std::vector<s_point> points;
            points.reserve(1 + 11 * 11);
            const auto add_point = [&](const float x, const float z)
            {
                float surface_y = 0.0f;
                if (!height_on_polygon(polygon, x, z, surface_y))
                {
                    return;
                }
                const float clearance = distance_to_polygon_edge_xz(
                    polygon.m_vertices, x, z);
                const float height_delta = std::abs(surface_y - reference_y);
                const auto duplicate = std::find_if(points.begin(), points.end(),
                    [&](const s_point& point)
                    {
                        const float dx = point.m_x - x;
                        const float dz = point.m_z - z;
                        return dx * dx + dz * dz <= 1.0e-6f;
                    });
                if (duplicate == points.end())
                {
                    points.push_back({ x, z, surface_y, clearance, height_delta });
                }
            };

            if (const auto interior = find_polygon_interior_point(polygon))
            {
                add_point((*interior)[0], (*interior)[1]);
            }

            constexpr int k_steps = 12;
            for (int zi = 1; zi < k_steps; ++zi)
            {
                const float z = polygon.m_bounds_min[2] +
                    (polygon.m_bounds_max[2] - polygon.m_bounds_min[2]) *
                    (static_cast<float>(zi) / static_cast<float>(k_steps));
                for (int xi = 1; xi < k_steps; ++xi)
                {
                    const float x = polygon.m_bounds_min[0] +
                        (polygon.m_bounds_max[0] - polygon.m_bounds_min[0]) *
                        (static_cast<float>(xi) / static_cast<float>(k_steps));
                    if (point_in_polygon_xz(polygon.m_vertices, x, z))
                    {
                        add_point(x, z);
                    }
                }
            }

            std::sort(points.begin(), points.end(),
                [](const s_point& a, const s_point& b)
                {
                    if (a.m_height_delta != b.m_height_delta)
                    {
                        return a.m_height_delta < b.m_height_delta;
                    }
                    return a.m_clearance > b.m_clearance;
                });

            const std::size_t point_count = std::min(
                points.size(), static_cast<std::size_t>(k_max_points_per_polygon));
            for (std::size_t point_index = 0; point_index < point_count; ++point_index)
            {
                const auto& point = points[point_index];
                s_rank rank;
                rank.m_height_delta = point.m_height_delta;
                rank.m_clearance = point.m_clearance;
                rank.m_area = polygon.m_projected_area;
                if (result && !is_better(rank, best_rank))
                {
                    // Candidate points are ordered by height delta and
                    // clearance, so later points on this polygon cannot win.
                    break;
                }
                if (!is_stage_capsule_supported(world, point.m_x, point.m_z, point.m_y,
                        capsule_radius, max_slope_degrees) ||
                    !is_stage_capsule_clear(world, point.m_x, point.m_z, point.m_y,
                        capsule_radius, capsule_height, max_slope_degrees))
                {
                    continue;
                }

                best_rank = rank;
                result = s_player_spawn{
                    { point.m_x, point.m_y + foot_offset, point.m_z },
                    polygon.m_normal, index, polygon.m_stage_node_index,
                    polygon.m_hit_object_index, polygon.m_projected_area,
                    polygon.m_object_name
                };
                break;
            }
        }
        return result;
    }

    bool is_stage_capsule_clear(
        const s_stage_collision_world& world, const float x, const float z,
        const float feet_y, const float capsule_radius, const float capsule_height,
        const float max_slope_degrees)
    {
        const float minimum_normal_y = std::cos(
            std::clamp(max_slope_degrees, 0.0f, 89.0f) * k_degrees_to_radians);
        return capsule_clear_impl(world, x, z, feet_y,
            capsule_radius, capsule_height, minimum_normal_y);
    }

    bool is_stage_capsule_supported(
        const s_stage_collision_world& world, const float x, const float z,
        const float surface_y, const float capsule_radius,
        const float max_slope_degrees, const float surface_tolerance)
    {
        const float tolerance = std::max(surface_tolerance, 0.01f);
        const auto centre = query_stage_ground(world, x, z, surface_y,
            tolerance, tolerance, max_slope_degrees);
        if (!centre || std::abs(centre->m_position[1] - surface_y) > tolerance)
        {
            return false;
        }

        const float radius = std::max(capsule_radius, 0.0f);
        if (radius <= k_epsilon)
        {
            return true;
        }
        const float normal_y = centre->m_normal[1];
        if (normal_y <= k_epsilon)
        {
            return false;
        }

        // Sample two rings. Queries are against the complete collision world,
        // not just the centre polygon, so adjacent triangles/quads can jointly
        // support a player whose capsule straddles their shared seam.
        //
        // Do not require neighbouring polygons to have nearly identical
        // normals: authored HIT ramps commonly meet flat floors or other
        // walkable ramps at a sharp, but continuous, crease. Instead, permit
        // the height difference that a walkable surface can produce over this
        // sample's horizontal distance. Every sample must still find a
        // walkable surface, which preserves ledge detection.
        constexpr int k_inner_samples = 12;
        constexpr int k_outer_samples = 24;
        constexpr float k_two_pi = 6.28318530717958647692f;
        const float maximum_slope = std::clamp(max_slope_degrees, 0.0f, 89.0f) *
            k_degrees_to_radians;
        const float maximum_slope_gradient = std::tan(maximum_slope);
        for (int ring = 0; ring < 2; ++ring)
        {
            const float ring_radius = radius * (ring == 0 ? 0.5f : 0.98f);
            const float sample_tolerance = std::max(
                tolerance, ring_radius * maximum_slope_gradient);
            const int sample_count = ring == 0 ? k_inner_samples : k_outer_samples;
            for (int sample_index = 0; sample_index < sample_count; ++sample_index)
            {
                const float angle = k_two_pi * static_cast<float>(sample_index) /
                    static_cast<float>(sample_count);
                const float offset_x = std::cos(angle) * ring_radius;
                const float offset_z = std::sin(angle) * ring_radius;
                const float expected_y = surface_y -
                    (centre->m_normal[0] * offset_x +
                        centre->m_normal[2] * offset_z) / normal_y;
                const auto sample = query_stage_ground(world, x + offset_x,
                    z + offset_z, expected_y, sample_tolerance,
                    sample_tolerance, max_slope_degrees);
                if (!sample ||
                    std::abs(sample->m_position[1] - expected_y) > sample_tolerance)
                {
                    return false;
                }
            }
        }
        return true;
    }

    std::array<float, 2> resolve_stage_horizontal_movement(
        const s_stage_collision_world& world, const float start_x, const float start_z,
        const float target_x, const float target_z, const float feet_y,
        const float capsule_radius, const float capsule_height,
        const float max_slope_degrees)
    {
        const float minimum_normal_y = std::cos(
            std::clamp(max_slope_degrees, 0.0f, 89.0f) * k_degrees_to_radians);
        float resolved_x = target_x;
        float resolved_z = target_z;
        const float radius = std::max(capsule_radius, 0.0f);
        const float height = std::max(capsule_height, 0.0f);
        const auto candidates = collect_collision_candidates(world,
            std::min(start_x, target_x) - radius,
            std::min(start_z, target_z) - radius,
            std::max(start_x, target_x) + radius,
            std::max(start_z, target_z) + radius);

        // HIT polygons with a near-vertical plane are blocking faces. Their
        // horizontal projection is a line segment, so test the swept player
        // centre against the face plane and its tangent/height extents.
        for (const std::size_t polygon_index : candidates)
        {
            const auto& polygon = world.m_polygons[polygon_index];
            if (polygon.m_normal[1] >= minimum_normal_y)
            {
                continue;
            }
            if (polygon.m_bounds_max[0] < std::min(start_x, target_x) - radius - k_epsilon ||
                polygon.m_bounds_min[0] > std::max(start_x, target_x) + radius + k_epsilon ||
                polygon.m_bounds_max[2] < std::min(start_z, target_z) - radius - k_epsilon ||
                polygon.m_bounds_min[2] > std::max(start_z, target_z) + radius + k_epsilon)
            {
                continue;
            }
            const float normal_length = std::sqrt(
                polygon.m_normal[0] * polygon.m_normal[0] +
                polygon.m_normal[2] * polygon.m_normal[2]);
            if (normal_length <= k_epsilon ||
                polygon.m_bounds_max[1] < feet_y ||
                polygon.m_bounds_min[1] > feet_y + height)
            {
                continue;
            }

            const float nx = polygon.m_normal[0] / normal_length;
            const float nz = polygon.m_normal[2] / normal_length;
            const auto& plane_origin = polygon.m_vertices.front();
            const float start_distance =
                (start_x - plane_origin[0]) * nx + (start_z - plane_origin[2]) * nz;
            const float end_distance =
                (resolved_x - plane_origin[0]) * nx + (resolved_z - plane_origin[2]) * nz;
            const float side = start_distance < 0.0f ? -1.0f : 1.0f;
            const float signed_start = start_distance * side;
            const float signed_end = end_distance * side;
            if (signed_end >= radius || signed_end >= signed_start)
            {
                continue;
            }

            const float denominator = signed_start - signed_end;
            const float fraction = denominator > k_epsilon
                ? std::clamp((signed_start - radius) / denominator, 0.0f, 1.0f)
                : 0.0f;
            const float contact_x = start_x + (resolved_x - start_x) * fraction;
            const float contact_z = start_z + (resolved_z - start_z) * fraction;
            const float tangent_x = -nz;
            const float tangent_z = nx;
            const float contact_tangent = contact_x * tangent_x + contact_z * tangent_z;
            float tangent_min = (std::numeric_limits<float>::max)();
            float tangent_max = (std::numeric_limits<float>::lowest)();
            for (const auto& vertex : polygon.m_vertices)
            {
                const float tangent = vertex[0] * tangent_x + vertex[2] * tangent_z;
                tangent_min = std::min(tangent_min, tangent);
                tangent_max = std::max(tangent_max, tangent);
            }
            if (contact_tangent < tangent_min - radius ||
                contact_tangent > tangent_max + radius)
            {
                continue;
            }

            // Remove only the movement component that penetrates the face.
            // This keeps tangential movement so the player can slide along walls.
            // A small slop avoids repeated floating-point contact.
            const float correction = radius - signed_end + 0.001f;
            resolved_x += nx * side * correction;
            resolved_z += nz * side * correction;
        }
        return { resolved_x, resolved_z };
    }

}