#include "render/stage_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "rasterizer/gl_helpers.hpp"
#include "rasterizer/math_helpers.hpp"

namespace
{
    // Lighting tuned to look closer to the editor: brighter ambient, two-sided
    // lighting so backfaces aren't dark, color material so tints/vertex colours
    // modulate, plus a fill light from the opposite side.
    void setup_stage_lighting()
    {
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glEnable(GL_COLOR_MATERIAL);

        GLfloat ambient[] = { 0.7f, 0.7f, 0.7f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

        GLfloat l0_pos[] = { 0.3f, 1.0f, 0.5f, 0.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, l0_pos);
        GLfloat l0_diff[] = { 0.9f, 0.9f, 0.9f, 1.0f };
        glLightfv(GL_LIGHT0, GL_DIFFUSE, l0_diff);
        GLfloat l0_spec[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        glLightfv(GL_LIGHT0, GL_SPECULAR, l0_spec);

        GLfloat l1_pos[] = { -0.7f, 0.2f, -0.5f, 0.0f };
        glLightfv(GL_LIGHT1, GL_POSITION, l1_pos);
        GLfloat l1_diff[] = { 0.5f, 0.5f, 0.5f, 1.0f };
        glLightfv(GL_LIGHT1, GL_DIFFUSE, l1_diff);
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

            // Frame the camera on the visible placed geometry (once; F re-fits).
            if (!ctx.camera->m_framed)
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
                    const_cast<s_camera&>(*ctx.camera).m_framed = true;
                    const_cast<s_camera&>(*ctx.camera).m_target = { (lo[0] + hi[0]) * 0.5f,
                        (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
                    const float extent = std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] });
                    const_cast<s_camera&>(*ctx.camera).m_distance = std::max(extent * 0.6f, 0.1f);
                    const_cast<s_camera&>(*ctx.camera).m_pitch = -30.0f; // look down on the stage
                    const_cast<s_camera&>(*ctx.camera).m_yaw = -30.0f; // angled view of the stage
                    std::cout << "Stage bbox: (" << lo[0] << "," << lo[1] << "," << lo[2] << ") to ("
                        << hi[0] << "," << hi[1] << "," << hi[2] << ") distance="
                        << ctx.camera->m_distance << '\n';
                }
            }

            setup_stage_lighting();
            const s_camera& camera = *ctx.camera;

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
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.5f);
            glColor3f(1.0f, 1.0f, 1.0f);
            for (const auto& e : opaque)
            {
                glPushMatrix();
                gl_mult_matrix(e.world);
                glCallList((*ctx.stage_mesh_lists)[ctx.stage_index][e.mesh_index]);
                glPopMatrix();
            }
            glDisable(GL_ALPHA_TEST);

            // Transparent pass: depth writes off, alpha blending on, back-to-front.
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            for (const auto& e : transparent)
            {
                glPushMatrix();
                gl_mult_matrix(e.world);
                glCallList((*ctx.stage_mesh_lists)[ctx.stage_index][e.mesh_index]);
                glPopMatrix();
            }

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glDisable(GL_LIGHTING);
        }
        else
        {
            // No model: draw the node hierarchy.
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.0f, 1.0f, 0.0f);
            glBegin(GL_LINES);
            const std::vector<std::vector<sdbz::mat4>>* stage_worlds = ctx.stage_worlds;
            for (std::size_t ni = 0; ni < stage_nodes.size(); ++ni)
            {
                const auto& node = stage_nodes[ni];
                if (node.m_parent < 0 || !group_visible(ni)) continue;
                const auto& p = (*stage_worlds)[ctx.stage_index][
                    static_cast<std::size_t>(node.m_parent)];
                const auto& c = (*stage_worlds)[ctx.stage_index][ni];
                glVertex3f(p[3], p[7], p[11]);
                glVertex3f(c[3], c[7], c[11]);
            }
            glEnd();
        }

        if (ctx.show_collision && ctx.has_stage_hit && ctx.stage_hits &&
            ctx.stage_index < ctx.stage_hits->size())
        {
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.1f, 1.0f, 0.3f);
            const std::vector<std::vector<sdbz::mat4>>* stage_worlds = ctx.stage_worlds;
            const std::unordered_map<std::string, std::size_t>* hit_by_names =
                ctx.stage_hit_by_names ? &(*ctx.stage_hit_by_names)[ctx.stage_index] : nullptr;
            for (std::size_t ni = 0; ni < stage_nodes.size(); ++ni)
            {
                const auto& node = stage_nodes[ni];
                if (node.m_kind != 3 || !group_visible(ni)) continue;
                glPushMatrix();
                gl_mult_matrix((*stage_worlds)[ctx.stage_index][ni]);
                glBegin(GL_LINES);
                for (const auto& reference : node.m_references)
                {
                    if (!hit_by_names) continue;
                    auto it = hit_by_names->find(reference.m_name);
                    if (it == hit_by_names->end()) continue;
                    const auto& object = (*ctx.stage_hits)[ctx.stage_index].m_objects[it->second];
                    for (std::uint32_t pi = 0; pi < object.m_polygon_count; ++pi)
                    {
                        const std::size_t polygon_index =
                            static_cast<std::size_t>(object.m_first_polygon) + pi;
                        if (polygon_index >=
                            (*ctx.stage_hits)[ctx.stage_index].m_polygons.size()) break;
                        const auto& polygon =
                            (*ctx.stage_hits)[ctx.stage_index].m_polygons[polygon_index];
                        for (std::uint32_t vi = 0; vi < polygon.m_vertex_count; ++vi)
                        {
                            const std::size_t a =
                                static_cast<std::size_t>(polygon.m_first_vertex) + vi;
                            const std::size_t b = static_cast<std::size_t>(
                                polygon.m_first_vertex) + ((vi + 1) % polygon.m_vertex_count);
                            if (a >= (*ctx.stage_hits)[ctx.stage_index].m_vertices.size() ||
                                b >= (*ctx.stage_hits)[ctx.stage_index].m_vertices.size()) continue;
                            const auto& pa = (*ctx.stage_hits)[ctx.stage_index].m_vertices[a].m_position;
                            const auto& pb = (*ctx.stage_hits)[ctx.stage_index].m_vertices[b].m_position;
                            glVertex3f(pa[0], pa[1], pa[2]);
                            glVertex3f(pb[0], pb[1], pb[2]);
                        }
                    }
                    glEnd();
                    glPopMatrix();
                }
            }
            glEnable(GL_LIGHTING);
        }
    }

}