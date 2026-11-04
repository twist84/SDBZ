#include "render/stage_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <print>
#include <span>
#include <unordered_map>
#include <vector>
#include <fstream>

#include "rasterizer/gl_helpers.hpp"
#include "rasterizer/math_helpers.hpp"
#include "rasterizer/c_rasterizer.hpp"

#include "gameplay/camera.hpp"
#include "core/stage_collision.hpp"

namespace
{
    // Lighting tuned to look closer to the editor: brighter ambient, two-sided
    // lighting so backfaces aren't dark, color material so tints/vertex colours
    // modulate, plus a fill light from the opposite side.
    void setup_stage_lighting()
    {
        c_rasterizer::enable(GL_LIGHTING);
        c_rasterizer::enable(GL_NORMALIZE);
        c_rasterizer::enable(GL_LIGHT0);
        c_rasterizer::enable(GL_LIGHT1);
        c_rasterizer::light_modeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
        c_rasterizer::color_material(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        c_rasterizer::enable(GL_COLOR_MATERIAL);

        GLfloat ambient[] = { 0.7f, 0.7f, 0.7f, 1.0f };
        c_rasterizer::light_modelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

        GLfloat l0_pos[] = { 0.3f, 1.0f, 0.5f, 0.0f };
        c_rasterizer::light_fv(GL_LIGHT0, GL_POSITION, l0_pos);
        GLfloat l0_diff[] = { 0.9f, 0.9f, 0.9f, 1.0f };
        c_rasterizer::light_fv(GL_LIGHT0, GL_DIFFUSE, l0_diff);
        GLfloat l0_spec[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        c_rasterizer::light_fv(GL_LIGHT0, GL_SPECULAR, l0_spec);

        GLfloat l1_pos[] = { -0.7f, 0.2f, -0.5f, 0.0f };
        c_rasterizer::light_fv(GL_LIGHT1, GL_POSITION, l1_pos);
        GLfloat l1_diff[] = { 0.5f, 0.5f, 0.5f, 1.0f };
        c_rasterizer::light_fv(GL_LIGHT1, GL_DIFFUSE, l1_diff);
    }

    bool material_is_transparent(const sdbz::s_model_material& mat)
    {
        return mat.m_colour_a[3] < 1.0f || mat.m_colour_b[3] < 1.0f;
    }

    std::array<float, 3> mesh_centroid_world(
        const sdbz::s_model_mesh& mesh,
        const std::array<float, 16>& world)
    {
        std::array<float, 3> c{ 0.0f, 0.0f, 0.0f };
        float count = 0.0f;
        for (const auto& p : mesh.m_primitives)
        {
            if (p.m_indices.size() < 3) continue;
            for (std::size_t i = 0; i < p.m_indices.size(); ++i)
            {
                const std::uint32_t idx = p.m_indices[i];
                if (idx >= mesh.m_vertices.size()) continue;
                const auto sp = transform_point(world, mesh.m_vertices[idx].m_position);
                c[0] += sp[0]; c[1] += sp[1]; c[2] += sp[2];
                count += 1.0f;
            }
        }
        if (count > 0.0f)
        {
            c[0] /= count; c[1] /= count; c[2] /= count;
        }
        return c;
    }

    float distance_sq(const std::array<float, 3>& a, const std::array<float, 3>& b)
    {
        const float dx = a[0] - b[0];
        const float dy = a[1] - b[1];
        const float dz = a[2] - b[2];
        return dx * dx + dy * dy + dz * dz;
    }
}

void render_stage_viewer(s_draw_context& ctx)
{
    // ---- Stage viewer ----
    if (ctx.has_stage && ctx.stages && ctx.stage_index < ctx.stages->size())
    {
        const auto group_visible = [&](std::size_t ni)
        {
            const char group = (*ctx.stage_groups)[ctx.stage_index][ni];
            return group == 0 || (group == 1 && ctx.show_effect) ||
                (group == 2 && ctx.show_templates);
        };

        const std::vector<sdbz::s_stage>& stages = *ctx.stages;
        const auto& stage_nodes = stages[ctx.stage_index].m_nodes;

        if (ctx.has_stage_model && ctx.stage_models &&
            ctx.stage_index < ctx.stage_models->size() &&
            (*ctx.stage_models_loaded)[ctx.stage_index])
        {
            if (ctx.stage_mesh_lists && ctx.stage_index < ctx.stage_mesh_lists->size() &&
                (*ctx.stage_mesh_lists)[ctx.stage_index].empty())
            {
                (*ctx.stage_mesh_lists)[ctx.stage_index].reserve(
                    (*ctx.stage_models)[ctx.stage_index].m_meshes.size());
                for (const auto& mesh : (*ctx.stage_models)[ctx.stage_index].m_meshes)
                {
                    (*ctx.stage_mesh_lists)[ctx.stage_index].push_back(
                        compile_static_mesh_list((*ctx.stage_models)[ctx.stage_index],
                            mesh, (*ctx.stage_textures)[ctx.stage_index]));
                }
            }

            // Frame only the independent world/viewer camera. Gameplay camera
            // follow must never be reset by stage auto-framing.
            sdbz::s_camera* framing_camera =
                const_cast<sdbz::s_camera*>(
                ctx.viewer_camera ? ctx.viewer_camera : ctx.camera);
            if (framing_camera && !framing_camera->m_framed)
            {
                std::array<float, 3> lo{ 1e30f, 1e30f, 1e30f };
                std::array<float, 3> hi{ -1e30f, -1e30f, -1e30f };
                bool any = false;
                const auto extend = [&](const std::array<float, 3>& p)
                {
                    for (int c = 0; c < 3; ++c)
                    {
                        lo[c] = std::min(lo[c], p[c]);
                        hi[c] = std::max(hi[c], p[c]);
                    }
                    any = true;
                };
                const std::vector<std::vector<sdbz::mat4>>* stage_worlds = ctx.stage_worlds;
                for (std::size_t ni = 0; ni < stage_nodes.size(); ++ni)
                {
                    const auto& node = stage_nodes[ni];
                    if (node.m_kind != 2) continue;
                    const std::unordered_map<std::string, std::vector<std::size_t>>* meshes_by_names =
                        ctx.stage_meshes_by_names ? &(*ctx.stage_meshes_by_names)[ctx.stage_index] : nullptr;
                    if (!meshes_by_names) continue;
                    auto it = meshes_by_names->find(node.m_name);
                    if (it == meshes_by_names->end()) continue;
                    for (const std::size_t mi : it->second)
                    {
                        for (const auto& v :
                            (*ctx.stage_models)[ctx.stage_index].m_meshes[mi].m_vertices)
                        {
                            extend(sdbz::mat4_point((*stage_worlds)[ctx.stage_index][ni], v.m_position));
                        }
                    }
                }
                if (any)
                {
                    framing_camera->m_framed = true;
                    framing_camera->m_target = { (lo[0] + hi[0]) * 0.5f,
                        (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
                    const float extent = std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] });
                    framing_camera->m_distance = std::max(extent * 0.6f, 0.1f);
                    framing_camera->m_pitch = -30.0f; // look down on the stage
                    framing_camera->m_yaw = -30.0f; // angled view of the stage
std::println("Stage bbox: ({}, {}, {}) to ({}, {}, {}) distance={}",
                        lo[0], lo[1], lo[2], hi[0], hi[1], hi[2],
                        framing_camera->m_distance);
                }
            }

            setup_stage_lighting();
            const sdbz::s_camera& camera = *ctx.camera;

            struct stage_entry
            {
                std::size_t mesh_index = 0;
                std::array<float, 16> world{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                std::array<float, 3> centroid{ 0,0,0 };
                bool transparent = false;
            };
            std::vector<stage_entry> opaque;
            std::vector<stage_entry> transparent;

            const std::vector<std::vector<sdbz::mat4>>* stage_worlds = ctx.stage_worlds;
            const std::vector<std::unordered_map<std::string, std::vector<std::size_t>>>*
                meshes_by_names = ctx.stage_meshes_by_names;
            const auto& model = (*ctx.stage_models)[ctx.stage_index];
            const auto& materials = model.m_materials;

            for (std::size_t ni = 0; ni < stage_nodes.size(); ++ni)
            {
                const auto& node = stage_nodes[ni];
                if (node.m_kind != 2 || !group_visible(ni)) continue;
                const std::unordered_map<std::string, std::vector<std::size_t>>* mbn =
                    meshes_by_names ? &(*meshes_by_names)[ctx.stage_index] : nullptr;
                if (!mbn) continue;
                auto it = mbn->find(node.m_name);
                if (it == mbn->end()) continue;
                const auto& world = (*stage_worlds)[ctx.stage_index][ni];
                for (const std::size_t mi : it->second)
                {
                    const auto& mesh = model.m_meshes[mi];
                    stage_entry e;
                    e.mesh_index = mi;
                    e.world = world;
                    e.centroid = mesh_centroid_world(mesh, world);
                    bool trans = false;
                    for (const auto& p : mesh.m_primitives)
                    {
                        if (p.m_indices.size() < 3) continue;
                        const std::int32_t mat_idx = p.m_material_index;
                        if (mat_idx >= 0 &&
                            static_cast<std::size_t>(mat_idx) < materials.size() &&
                            material_is_transparent(materials[mat_idx]))
                        {
                            trans = true;
                            break;
                        }
                    }
                    e.transparent = trans;
                    if (trans) transparent.push_back(e);
                    else opaque.push_back(e);
                }
            }

            // Sort transparent entries back-to-front from the camera target.
            std::sort(transparent.begin(), transparent.end(),
                [&camera](const stage_entry& a, const stage_entry& b)
                {
                    return distance_sq(a.centroid, camera.m_target) >
                        distance_sq(b.centroid, camera.m_target);
                });

            // Opaque pass: depth writes on, no blending. Alpha test discards
            // cutout foliage holes so they don't write depth, while opaque parts
            // still write depth for correct zbuffer ordering.
            c_rasterizer::depth_mask(GL_TRUE);
            c_rasterizer::disable(GL_BLEND);
            c_rasterizer::disable(GL_TEXTURE_2D);
            c_rasterizer::enable(GL_ALPHA_TEST);
            c_rasterizer::alpha_func(GL_GREATER, 0.5f);
            c_rasterizer::color3f(1.0f, 1.0f, 1.0f);
            for (const auto& e : opaque)
            {
                c_rasterizer::push_matrix();
                gl_mult_matrix(e.world);
                c_rasterizer::call_list((*ctx.stage_mesh_lists)[ctx.stage_index][e.mesh_index]);
                c_rasterizer::pop_matrix();
            }
            c_rasterizer::disable(GL_ALPHA_TEST);

            // Transparent pass: depth writes off, alpha blending on, back-to-front.
            c_rasterizer::depth_mask(GL_FALSE);
            c_rasterizer::enable(GL_BLEND);
            c_rasterizer::blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (const auto& e : transparent)
            {
                c_rasterizer::push_matrix();
                gl_mult_matrix(e.world);
                c_rasterizer::call_list((*ctx.stage_mesh_lists)[ctx.stage_index][e.mesh_index]);
                c_rasterizer::pop_matrix();
            }

            c_rasterizer::depth_mask(GL_TRUE);
            c_rasterizer::disable(GL_BLEND);
            c_rasterizer::disable(GL_LIGHTING);
        }
        else
        {
            // No model: draw the node hierarchy.
            c_rasterizer::disable(GL_LIGHTING);
            c_rasterizer::disable(GL_TEXTURE_2D);
            c_rasterizer::color3f(0.0f, 1.0f, 0.0f);
            c_rasterizer::begin(GL_LINES);
            const std::vector<std::vector<sdbz::mat4>>* stage_worlds = ctx.stage_worlds;
            for (std::size_t ni = 0; ni < stage_nodes.size(); ++ni)
            {
                const auto& node = stage_nodes[ni];
                if (node.m_parent < 0 || !group_visible(ni)) continue;
                const auto& p = (*stage_worlds)[ctx.stage_index][
                    static_cast<std::size_t>(node.m_parent)];
                const auto& c = (*stage_worlds)[ctx.stage_index][ni];
                c_rasterizer::vertex3f(p[3], p[7], p[11]);
                c_rasterizer::vertex3f(c[3], c[7], c[11]);
            }
            c_rasterizer::end();
        }

        if (ctx.show_collision && ctx.stage_collision_world)
        {
            c_rasterizer::disable(GL_LIGHTING);
            c_rasterizer::disable(GL_TEXTURE_2D);
            c_rasterizer::begin(GL_LINES);
            for (std::size_t polygon_index = 0;
                polygon_index < ctx.stage_collision_world->m_polygons.size(); ++polygon_index)
            {
                const auto& polygon = ctx.stage_collision_world->m_polygons[polygon_index];
                if (ctx.player_controller &&
                    ctx.player_controller->m_ground_polygon_index == polygon_index)
                {
                    c_rasterizer::color3f(1.0f, 0.85f, 0.05f);
                }
                else if (polygon.m_normal[1] >= 0.6427876f)
                {
                    // Green = walkable; red = steep/wall collision.
                    c_rasterizer::color3f(0.1f, 1.0f, 0.3f);
                }
                else
                {
                    c_rasterizer::color3f(1.0f, 0.2f, 0.15f);
                }
                for (std::size_t vertex_index = 0;
                    vertex_index < polygon.m_vertices.size(); ++vertex_index)
                {
                    const auto& a = polygon.m_vertices[vertex_index];
                    const auto& b = polygon.m_vertices[
                        (vertex_index + 1) % polygon.m_vertices.size()];
                    c_rasterizer::vertex3f(a[0], a[1], a[2]);
                    c_rasterizer::vertex3f(b[0], b[1], b[2]);
                }
            }
            c_rasterizer::end();
            c_rasterizer::enable(GL_LIGHTING);
        }

        if (ctx.show_collision && ctx.player_actor_placed && ctx.player_controller)
        {
            const auto& player = *ctx.player_controller;
            const float radius = std::max(player.m_collision_capsule_radius, 0.0f);
            const float height = std::max(player.m_collision_capsule_height, 0.0f);
            const float cap_radius = std::min(radius, height * 0.5f);
            const float feet_y = player.m_position[1] - player.m_collision_foot_offset;
            constexpr int k_segments = 24;
            constexpr int k_hemi_rings = 8;
            constexpr float k_pi = 3.14159265358979323846f;
            constexpr float k_two_pi = 6.28318530717958647692f;
            const float cylinder_bottom = feet_y + cap_radius;
            const float cylinder_top = feet_y + height - cap_radius;
            c_rasterizer::disable(GL_LIGHTING);
            c_rasterizer::disable(GL_TEXTURE_2D);
            c_rasterizer::color3f(1.0f, 0.75f, 0.05f);
            // Bottom hemisphere: rings from feet_y up to feet_y + radius.
            for (int ring = 0; ring <= k_hemi_rings; ++ring)
            {
                const float t = static_cast<float>(ring) /
                    static_cast<float>(k_hemi_rings);
                const float theta = t * k_pi * 0.5f;
                const float hemi_y = feet_y + cap_radius * (1.0f - std::cos(theta));
                const float hemi_r = cap_radius * std::sin(theta);
                c_rasterizer::begin(GL_LINE_LOOP);
                for (int segment = 0; segment < k_segments; ++segment)
                {
                    const float angle = k_two_pi * static_cast<float>(segment) /
                        static_cast<float>(k_segments);
                    c_rasterizer::vertex3f(
                        player.m_position[0] + std::cos(angle) * hemi_r,
                        hemi_y,
                        player.m_position[2] + std::sin(angle) * hemi_r);
                }
                c_rasterizer::end();
            }
            // Top hemisphere: rings from feet_y + height - radius up to feet_y + height.
            for (int ring = 0; ring <= k_hemi_rings; ++ring)
            {
                const float t = static_cast<float>(ring) /
                    static_cast<float>(k_hemi_rings);
                const float theta = t * k_pi * 0.5f;
                const float hemi_y = feet_y + height - cap_radius * (1.0f - std::cos(theta));
                const float hemi_r = cap_radius * std::sin(theta);
                c_rasterizer::begin(GL_LINE_LOOP);
                for (int segment = 0; segment < k_segments; ++segment)
                {
                    const float angle = k_two_pi * static_cast<float>(segment) /
                        static_cast<float>(k_segments);
                    c_rasterizer::vertex3f(
                        player.m_position[0] + std::cos(angle) * hemi_r,
                        hemi_y,
                        player.m_position[2] + std::sin(angle) * hemi_r);
                }
                c_rasterizer::end();
            }
            // Cylinder: vertical lines from cylinder_bottom to cylinder_top.
            c_rasterizer::begin(GL_LINES);
            for (int segment = 0; segment < 4; ++segment)
            {
                const float angle = k_two_pi * static_cast<float>(segment) / 4.0f;
                const float x = std::cos(angle) * cap_radius;
                const float z = std::sin(angle) * cap_radius;
                c_rasterizer::vertex3f(player.m_position[0] + x,
                    cylinder_bottom, player.m_position[2] + z);
                c_rasterizer::vertex3f(player.m_position[0] + x,
                    cylinder_top, player.m_position[2] + z);
            }
            c_rasterizer::vertex3f(player.m_position[0], cylinder_bottom,
                player.m_position[2]);
            c_rasterizer::vertex3f(player.m_position[0], cylinder_top,
                player.m_position[2]);
            c_rasterizer::end();
            c_rasterizer::begin(GL_LINES);
            c_rasterizer::vertex3f(player.m_position[0], cylinder_bottom,
                player.m_position[2]);
            c_rasterizer::vertex3f(
                player.m_position[0] + player.m_ground_normal[0] * 0.75f,
                player.m_position[1] + player.m_ground_normal[1] * 0.75f,
                player.m_position[2] + player.m_ground_normal[2] * 0.75f);
            c_rasterizer::end();
            c_rasterizer::enable(GL_LIGHTING);
        }
    }

}
