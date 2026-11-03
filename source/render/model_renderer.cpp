#include "render/model_renderer.hpp"

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

#include "rasterizer/math_helpers.hpp"
#include "rasterizer/c_rasterizer.hpp"

namespace
{
    void setup_model_lighting()
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

        GLfloat l0_pos[] = { 5.0f, 10.0f, 5.0f, 0.0f };
        c_rasterizer::light_fv(GL_LIGHT0, GL_POSITION, l0_pos);
        GLfloat l0_diff[] = { 0.9f, 0.9f, 0.9f, 1.0f };
        c_rasterizer::light_fv(GL_LIGHT0, GL_DIFFUSE, l0_diff);
        GLfloat l0_spec[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        c_rasterizer::light_fv(GL_LIGHT0, GL_SPECULAR, l0_spec);

        GLfloat l1_pos[] = { -5.0f, 2.0f, -5.0f, 0.0f };
        c_rasterizer::light_fv(GL_LIGHT1, GL_POSITION, l1_pos);
        GLfloat l1_diff[] = { 0.5f, 0.5f, 0.5f, 1.0f };
        c_rasterizer::light_fv(GL_LIGHT1, GL_DIFFUSE, l1_diff);
    }

    bool material_is_transparent(const sdbz::s_model_material& mat)
    {
        return mat.m_colour_a[3] < 1.0f || mat.m_colour_b[3] < 1.0f;
    }

    struct model_entry
    {
        std::size_t mesh_index = 0;
        std::size_t primitive_index = 0;
        std::array<float, 3> centroid{ 0,0,0 };
        bool transparent = false;
    };

    float distance_sq(const std::array<float, 3>& a, const std::array<float, 3>& b)
    {
        const float dx = a[0] - b[0];
        const float dy = a[1] - b[1];
        const float dz = a[2] - b[2];
        return dx * dx + dy * dy + dz * dz;
    }
}

void render_model_viewer(s_draw_context& ctx)
{
    // ---- Model viewer ----
    if (ctx.has_model && ctx.model && !ctx.model->m_meshes.empty())
    {
        const std::size_t node_count = ctx.model->m_nodes.size();
        std::vector<std::array<float, 16>> local(node_count);
        for (std::size_t ni = 0; ni < node_count; ++ni)
        {
            local[ni] = node_local_mat(ctx.model->m_nodes[ni]);
        }

        // bind pose world transforms (for drawing bones that match mesh vertices).
        std::vector<std::array<float, 16>> bind_local(node_count);
        for (std::size_t ni = 0; ni < node_count; ++ni)
        {
            bind_local[ni] = node_local_mat(ctx.model->m_nodes[ni]);
        }
        std::vector<std::array<float, 16>> bind_world = build_world(bind_local, ctx.model->m_nodes);
        std::vector<std::array<float, 16>> inverse_bind(bind_world.size());
        for (std::size_t i = 0; i < bind_world.size(); ++i)
        {
            inverse_bind[i] = mat4_inverse(bind_world[i]);
        }

        // local transform per node: animation sample overrides bind pose.
        std::vector<std::array<float, 16>> local_xform(node_count);
        for (std::size_t ni = 0; ni < node_count; ++ni)
        {
            local_xform[ni] = node_local_mat(ctx.model->m_nodes[ni]);
        }

        if (ctx.has_motion && ctx.motion && ctx.motion_clip_index < ctx.motion->m_motions.size())
        {
            const auto& clip = ctx.motion->m_motions[ctx.motion_clip_index];
            std::unordered_map<std::string, std::size_t> name_to_node;
            for (std::size_t ni = 0; ni < node_count; ++ni)
            {
                name_to_node[ctx.model->m_nodes[ni].m_name] = ni;
            }
            for (const auto& an : clip.m_nodes)
            {
                auto it = name_to_node.find(an.m_name);
                if (it == name_to_node.end()) continue;
                const std::size_t mi = it->second;
                std::array<float, 3> trans{ 0.0f, 0.0f, 0.0f };
                std::array<float, 4> rot{ 0.0f, 0.0f, 0.0f, 1.0f };
                std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
                if (!an.m_translation.m_values.empty())
                {
                    const auto t = sdbz::sample_animation_track(an.m_translation, ctx.animation_time, false);
                    trans = { t[0], t[1], t[2] };
                }
                if (!an.m_rotation.m_values.empty())
                {
                    const auto r = sdbz::sample_animation_track(an.m_rotation, ctx.animation_time, true);
                    rot = { r[0], r[1], r[2], r[3] };
                }
                if (!an.m_scale.m_values.empty())
                {
                    const auto s = sdbz::sample_animation_track(an.m_scale, ctx.animation_time, false);
                    scale = { s[0], s[1], s[2] };
                }
                const float x = rot[0], y = rot[1], z = rot[2], w = rot[3];
                std::array<float, 16> rot_mat{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                const float xx = x * x, yy = y * y, zz = z * z;
                const float xy = x * y, xz = x * z, xw = x * w;
                const float yz = y * z, yw = y * w;
                const float zw = z * w;
                rot_mat[0] = 1.0f - 2.0f * (yy + zz);
                rot_mat[1] = 2.0f * (xy - zw);
                rot_mat[2] = 2.0f * (xz + yw);
                rot_mat[4] = 2.0f * (xy + zw);
                rot_mat[5] = 1.0f - 2.0f * (xx + zz);
                rot_mat[6] = 2.0f * (yz - xw);
                rot_mat[8] = 2.0f * (xz - yw);
                rot_mat[9] = 2.0f * (yz + xw);
                rot_mat[10] = 1.0f - 2.0f * (xx + yy);
                std::array<float, 16> trans_mat{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                trans_mat[3] = trans[0]; trans_mat[7] = trans[1]; trans_mat[11] = trans[2];
                std::array<float, 16> scale_mat{ scale[0],0,0,0, 0,scale[1],0,0, 0,0,scale[2],0, 0,0,0,1 };
                local_xform[mi] = multiply4(multiply4(trans_mat, rot_mat), scale_mat);
            }
        }

        std::vector<std::array<float, 16>> world = build_world(local_xform, ctx.model->m_nodes);

        // Compute skinned bounding box for camera framing.
        if (!ctx.camera->m_framed)
        {
            std::array<float, 3> skinned_min{ 1e9f, 1e9f, 1e9f };
            std::array<float, 3> skinned_max{ -1e9f, -1e9f, -1e9f };
            bool skinned_has_data = false;
            static const std::vector<sdbz::s_model_mesh> k_no_meshes;
            for (const auto& m : ctx.model->m_meshes)
            {
                if (ctx.mesh_visible && !(*ctx.mesh_visible)[
                    static_cast<std::size_t>(&m - &ctx.model->m_meshes[0])]) continue;
                if (!ctx.mesh_filters.empty())
                {
                    bool matches = false;
                    for (const auto& f : ctx.mesh_filters)
                    {
                        if (m.m_name.find(f) != std::string::npos)
                        {
                            matches = true;
                            break;
                        }
                    }
                    if (!matches) continue;
                }
                for (const auto& p : m.m_primitives)
                {
                    if (p.m_indices.size() < 3) continue;
                    for (std::size_t i = 0; i + 2 < p.m_indices.size(); i += 3)
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            const std::uint32_t idx = p.m_indices[i + k];
                            if (idx >= m.m_vertices.size()) continue;
                            const auto& v = m.m_vertices[idx];
                            const auto sp = skin_position(v.m_position, v, m.m_rigid_node_index,
                                inverse_bind, world, ctx.model->m_nodes);
                            skinned_min[0] = std::min(skinned_min[0], sp[0]);
                            skinned_min[1] = std::min(skinned_min[1], sp[1]);
                            skinned_min[2] = std::min(skinned_min[2], sp[2]);
                            skinned_max[0] = std::max(skinned_max[0], sp[0]);
                            skinned_max[1] = std::max(skinned_max[1], sp[1]);
                            skinned_max[2] = std::max(skinned_max[2], sp[2]);
                            skinned_has_data = true;
                        }
                    }
                }
            }
            if (skinned_has_data)
            {
                const_cast<s_camera&>(*ctx.camera).m_framed = true;
                const_cast<s_camera&>(*ctx.camera).m_target = {
                    (skinned_min[0] + skinned_max[0]) * 0.5f,
                    (skinned_min[1] + skinned_max[1]) * 0.5f,
                    (skinned_min[2] + skinned_max[2]) * 0.5f,
                };
                const float extent = std::max({ skinned_max[0] - skinned_min[0],
                    skinned_max[1] - skinned_min[1],
                    skinned_max[2] - skinned_min[2] });
                const_cast<s_camera&>(*ctx.camera).m_distance = std::max(extent * 0.75f, 0.1f);
                std::print("Skinned bbox: ({},{},{}) to ({},{},{}) extent={} distance={}s\n",
                    skinned_min[0], skinned_min[1], skinned_min[2],
                    skinned_max[0], skinned_max[1], skinned_max[2], extent,
                    ctx.camera->m_distance);
            }
        }

        setup_model_lighting();
        const s_camera& camera = *ctx.camera;
        const auto& materials = ctx.model->m_materials;

        // Collect primitives, classifying opaque vs transparent.
        std::vector<model_entry> opaque;
        std::vector<model_entry> transparent;
        for (std::size_t mi = 0; mi < ctx.model->m_meshes.size(); ++mi)
        {
            const auto& mesh = ctx.model->m_meshes[mi];
            if (!ctx.mesh_filters.empty())
            {
                bool matches = false;
                for (const auto& filter : ctx.mesh_filters)
                {
                    if (mesh.m_name.find(filter) != std::string::npos)
                    {
                        matches = true;
                        break;
                    }
                }
                if (!matches) continue;
            }
            if (ctx.mesh_visible && !(*ctx.mesh_visible)[mi]) continue;
            const std::int32_t rigid_node = mesh.m_rigid_node_index;
            for (std::size_t pi = 0; pi < mesh.m_primitives.size(); ++pi)
            {
                const auto& p = mesh.m_primitives[pi];
                if (p.m_indices.size() < 3) continue;

                // World-space centroid (skinned approx) for transparency sorting.
                std::array<float, 3> c{ 0.0f, 0.0f, 0.0f };
                float count = 0.0f;
                for (std::uint32_t idx : p.m_indices)
                {
                    if (idx >= mesh.m_vertices.size()) continue;
                    const auto& v = mesh.m_vertices[idx];
                    const auto sp = skin_position(v.m_position, v, rigid_node,
                        inverse_bind, world, ctx.model->m_nodes);
                    c[0] += sp[0]; c[1] += sp[1]; c[2] += sp[2];
                    count += 1.0f;
                }
                if (count > 0.0f)
                {
                    c[0] /= count; c[1] /= count; c[2] /= count;
                }

                bool trans = false;
                const std::int32_t mat_idx = p.m_material_index;
                if (mat_idx >= 0 &&
                    static_cast<std::size_t>(mat_idx) < materials.size() &&
                    material_is_transparent(materials[mat_idx]))
                {
                    trans = true;
                }
                // Vertex-colour alpha < 1 also indicates transparency.
                if (!trans)
                {
                    for (std::uint32_t idx : p.m_indices)
                    {
                        if (idx >= mesh.m_vertices.size()) continue;
                        const auto& v = mesh.m_vertices[idx];
                        if (v.m_has_colour && v.m_colour[3] < 1.0f)
                        {
                            trans = true;
                            break;
                        }
                    }
                }
                model_entry e;
                e.mesh_index = mi;
                e.primitive_index = pi;
                e.centroid = c;
                e.transparent = trans;
                if (trans) transparent.push_back(e);
                else opaque.push_back(e);
            }
        }

        // Sort transparent primitives back-to-front from the camera target.
        std::sort(transparent.begin(), transparent.end(),
            [&camera](const model_entry& a, const model_entry& b)
            {
                return distance_sq(a.centroid, camera.m_target) >
                    distance_sq(b.centroid, camera.m_target);
            });

        auto draw_primitive = [&](const model_entry& e)
        {
            const auto& mesh = ctx.model->m_meshes[e.mesh_index];
            const auto& p = mesh.m_primitives[e.primitive_index];
            const std::int32_t material_index = p.m_material_index;
            bool has_texture = false;
            GLuint tex = 0;
            if (ctx.has_textures && ctx.texture_names && material_index >= 0 &&
                static_cast<std::size_t>(material_index) < ctx.model->m_materials.size())
            {
                const std::int32_t texture_index =
                    ctx.model->m_materials[material_index].m_texture_index;
                if (texture_index >= 0 &&
                    static_cast<std::size_t>(texture_index) < ctx.texture_names->size())
                {
                    tex = (*ctx.texture_names)[texture_index];
                    has_texture = true;
                }
            }
            if (has_texture)
            {
                c_rasterizer::enable(GL_TEXTURE_2D);
                c_rasterizer::bind_texture(GL_TEXTURE_2D, tex);
            }
            else
            {
                c_rasterizer::disable(GL_TEXTURE_2D);
            }
            c_rasterizer::begin(GL_TRIANGLES);
            const std::int32_t rigid_node = mesh.m_rigid_node_index;
            for (std::uint32_t idx : p.m_indices)
            {
                if (idx >= mesh.m_vertices.size()) continue;
                const auto& v = mesh.m_vertices[idx];
                const auto sp = skin_position(v.m_position, v, rigid_node,
                    inverse_bind, world, ctx.model->m_nodes);
                c_rasterizer::normal3f(v.m_normal[0], v.m_normal[1], v.m_normal[2]);
                if (v.m_has_colour)
                {
                    c_rasterizer::color4f(v.m_colour[0], v.m_colour[1], v.m_colour[2], v.m_colour[3]);
                }
                if (has_texture) c_rasterizer::tex_coord2f(v.m_uv[0], v.m_uv[1]);
                c_rasterizer::vertex3f(sp[0], sp[1], sp[2]);
            }
            c_rasterizer::end();
            if (has_texture)
            {
                c_rasterizer::disable(GL_TEXTURE_2D);
            }
        };

        // Opaque pass: depth writes on, no blending. Alpha test discards cutout
        // foliage holes so opaque parts still write depth for correct zbuffer.
        c_rasterizer::depth_mask(GL_TRUE);
        c_rasterizer::disable(GL_BLEND);
        c_rasterizer::enable(GL_ALPHA_TEST);
        c_rasterizer::alpha_func(GL_GREATER, 0.5f);
        c_rasterizer::color3f(1.0f, 1.0f, 1.0f);
        for (const auto& e : opaque)
        {
            draw_primitive(e);
        }
        c_rasterizer::disable(GL_ALPHA_TEST);

        // Transparent pass: depth writes off, alpha blending on, back-to-front.
        c_rasterizer::depth_mask(GL_FALSE);
        c_rasterizer::enable(GL_BLEND);
        c_rasterizer::blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (const auto& e : transparent)
        {
            draw_primitive(e);
        }

        c_rasterizer::depth_mask(GL_TRUE);
        c_rasterizer::disable(GL_BLEND);
        c_rasterizer::disable(GL_LIGHTING);
    }

}
