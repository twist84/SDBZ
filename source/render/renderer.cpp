#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>

namespace
{
// Small row-major 4x4 helpers (translation in [3], [7], [11]).

std::array<float, 16> node_local_mat(const sdbz::s_model_node& n)
{
    constexpr float k_deg_to_rad = 0.01745329251994329577f;
    const float x = n.m_rotation_degrees[0] * k_deg_to_rad * 0.5f;
    const float y = n.m_rotation_degrees[1] * k_deg_to_rad * 0.5f;
    const float z = n.m_rotation_degrees[2] * k_deg_to_rad * 0.5f;
    const float sx = std::sin(x), sy = std::sin(y), sz = std::sin(z);
    const float cx = std::cos(x), cy = std::cos(y), cz = std::cos(z);
    // Quaternion from Euler (matches arika-py quaternion function).
    const float qx = sx * cy * cz - cx * sy * sz;
    const float qy = cx * sy * cz + sx * cy * sz;
    const float qz = cx * cy * sz - sx * sy * cz;
    const float qw = cx * cy * cz + sx * sy * sz;
    // Quaternion to matrix (matches arika-py local_matrix).
    const float x2 = qx * qx, y2 = qy * qy, z2 = qz * qz;
    const float xy = qx * qy, xz = qx * qz, xw = qx * qw;
    const float yz = qy * qz, yw = qy * qw;
    const float zw = qz * qw;
    std::array<float, 16> r{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    r[0] = n.m_scale[0] * (1 - 2 * (y2 + z2));
    r[1] = n.m_scale[1] * 2 * (xy - zw);
    r[2] = n.m_scale[2] * 2 * (xz + yw);
    r[4] = n.m_scale[0] * 2 * (xy + zw);
    r[5] = n.m_scale[1] * (1 - 2 * (x2 + z2));
    r[6] = n.m_scale[2] * 2 * (yz - xw);
    r[8] = n.m_scale[0] * 2 * (xz - yw);
    r[9] = n.m_scale[1] * 2 * (yz + xw);
    r[10] = n.m_scale[2] * (1 - 2 * (x2 + y2));
    r[3] = n.m_translation[0];
    r[7] = n.m_translation[1];
    r[11] = n.m_translation[2];
    return r;
}

std::array<float, 16> multiply4(const std::array<float, 16>& a,
    const std::array<float, 16>& b)
{
    std::array<float, 16> out{};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            float v = 0.0f;
            for (int k = 0; k < 4; ++k) v += a[row * 4 + k] * b[k * 4 + col];
            out[row * 4 + col] = v;
        }
    }
    return out;
}

std::array<float, 3> transform_point(const std::array<float, 16>& m,
    const std::array<float, 3>& p)
{
    return std::array<float, 3>{ {
        m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3],
        m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7],
        m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]
    } };
}

std::vector<std::array<float, 16>> build_world(
    const std::vector<std::array<float, 16>>& local,
    const std::vector<sdbz::s_model_node>& nodes)
{
    std::vector<std::array<float, 16>> w(local.size());
    std::vector<char> done(local.size(), 0);
    std::function<std::array<float, 16>(std::size_t)> build =
        [&](std::size_t index) -> std::array<float, 16>
    {
        if (done[index] == 2) return w[index];
        if (done[index] == 1) throw std::runtime_error("hierarchy cycle");
        done[index] = 1;
        const auto local_mat = local[index];
        const auto& node = nodes[index];
        std::array<float, 16> result;
        if (node.m_parent >= 0)
        {
            const auto& parent_world = build(static_cast<std::size_t>(node.m_parent));
            result = multiply4(parent_world, local_mat);
        }
        else
        {
            result = local_mat;
        }
        w[index] = result;
        done[index] = 2;
        return result;
    };
    for (std::size_t index = 0; index < local.size(); ++index) build(index);
    return w;
}

std::array<float, 16> mat4_inverse(const std::array<float, 16>& m)
{
    std::array<std::array<float, 8>, 4> aug{};
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c) aug[r][c] = m[r * 4 + c];
        aug[r][4 + r] = 1.0f;
    }
    for (int c = 0; c < 4; ++c)
    {
        int piv = c; float maxv = std::fabs(aug[c][c]);
        for (int r = c + 1; r < 4; ++r)
            if (std::fabs(aug[r][c]) > maxv) { maxv = std::fabs(aug[r][c]); piv = r; }
        if (maxv < 1e-6f) return std::array<float, 16>{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        if (piv != c) for (int k = 0; k < 8; ++k) std::swap(aug[c][k], aug[piv][k]);
        float inv = 1.0f / aug[c][c];
        for (int k = 0; k < 8; ++k) aug[c][k] *= inv;
        for (int r = 0; r < 4; ++r)
        {
            if (r == c) continue;
            float f = aug[r][c];
            if (f == 0.0f) continue;
            for (int k = 0; k < 8; ++k) aug[r][k] -= f * aug[c][k];
        }
    }
    std::array<float, 16> inv{};
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) inv[r * 4 + c] = aug[r][c + 4];
    return inv;
}

std::array<float, 3> skin_position(const std::array<float, 3>& pos,
    const sdbz::s_model_vertex& v, std::int32_t rigid_node,
    const std::vector<std::array<float, 16>>& inverse_bind,
    const std::vector<std::array<float, 16>>& world,
    const std::vector<sdbz::s_model_node>& nodes)
{
    // Per arika-py: skinned mesh vertices are in joint-local space at bind pose.
    // Animated: world[joint] * inverse_bind[joint] * pos, weighted by bone weights.
    std::array<float, 3> out{ 0.0f, 0.0f, 0.0f };
    float total_weight = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float w = v.m_weights[i];
        if (w <= 0.0f) continue;
        const std::uint16_t joint = v.m_joints[i];
        if (joint >= nodes.size()) continue;
        // bind pose -> joint-local (inverse_bind) -> animated world (world[joint]).
        const auto bone_local = transform_point(inverse_bind[joint], pos);
        const auto skinned = transform_point(world[joint], bone_local);
        out[0] += w * skinned[0];
        out[1] += w * skinned[1];
        out[2] += w * skinned[2];
        total_weight += w;
    }
    if (total_weight > 0.0f)
    {
        out[0] /= total_weight;
        out[1] /= total_weight;
        out[2] /= total_weight;
    }
    else
    {
        // Non-skinned mesh: apply node world transform.
        // Rigid meshes (head, hair, hands...) are attached to the node stored on the
        // mesh, not in the per-vertex joint array (which is always 0 for them).
        std::size_t joint = rigid_node >= 0 ? static_cast<std::size_t>(rigid_node) : 0;
        if (joint >= nodes.size()) joint = 0;
        out = transform_point(world[joint], pos);
    }
    return out;
}
}

void upload_glyph_textures(const s_ffon_font& font, std::vector<GLuint>& glyph_textures)
{
    // Upload each 4bpp glyph bitmap expanded to RGBA.
    glyph_textures.resize(font.glyph_count);
    for (std::uint32_t gi = 0; gi < font.glyph_count; ++gi)
    {
        const auto& data = font.glyphs[gi];
        const std::uint8_t width = font.width;
        const std::uint8_t height = font.height;
        const std::uint8_t row_stride = font.bytes_per_row;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Expand 4bpp to RGBA.
        std::vector<std::uint8_t> rgba(width * height * 4);
        for (std::uint32_t y = 0; y < height; ++y)
        {
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const auto packed = data[y * row_stride + (x / 2)];
                const auto value = (x & 1) ? (packed >> 4) : (packed & 0x0F);
                const auto intensity = static_cast<std::uint8_t>(value * 17);
                const auto base = (y * width + x) * 4;
                rgba[base] = 255;
                rgba[base + 1] = 255;
                rgba[base + 2] = 255;
                rgba[base + 3] = intensity; // coverage; needs GL_BLEND
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, rgba.data());
        glyph_textures[gi] = tex;
    }
}

void begin_2d(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void end_2d()
{
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void draw_glyph(GLuint tex, float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x + w, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x + w, y + h);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void draw_text(const std::string& text, float x, float y, float scale,
    std::uint8_t r, std::uint8_t g, std::uint8_t b,
    const s_ffon_font& font, const std::vector<GLuint>& glyph_textures)
{
    if (font.char_map.empty() || glyph_textures.empty()) return;
    constexpr int k_spacing = 3;
    const int space_width = std::max(8, static_cast<int>(font.width) / 3);
    float cursor_x = std::floor(x);
    const float top = std::floor(y);
    for (unsigned char ch : text)
    {
        if (ch < 32 || ch > 126) ch = 32;
        if (ch == ' ')
        {
            cursor_x += (space_width + k_spacing) * scale;
            continue;
        }
        const std::size_t slot = static_cast<std::size_t>(ch - 32);
        const auto glyph_id = slot < font.char_map.size() ? font.char_map[slot] : 0;
        if (glyph_id == 0 || static_cast<std::size_t>(glyph_id) > glyph_textures.size()) continue;
        const auto [left, right] = glyph_ink_bounds(font, font.glyphs[glyph_id - 1]);
        if (right <= left) continue;
        draw_glyph(glyph_textures[glyph_id - 1], cursor_x - left * scale, top,
            static_cast<float>(font.width) * scale, static_cast<float>(font.height) * scale, r, g, b);
        cursor_x += ((right - left) + k_spacing) * scale;
    }
}

void gl_mult_matrix(const sdbz::mat4& m)
{
    const GLfloat column_major[16] = {
        m[0], m[4], m[8],  m[12],
        m[1], m[5], m[9],  m[13],
        m[2], m[6], m[10], m[14],
        m[3], m[7], m[11], m[15] };
    glMultMatrixf(column_major);
}

GLuint compile_static_mesh_list(const sdbz::s_model& model,
    const sdbz::s_model_mesh& mesh,
    const std::vector<GLuint>& textures)
{
    const GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    for (const auto& primitive : mesh.m_primitives)
    {
        if (primitive.m_indices.size() < 3) continue;
        bool has_texture = false;
        GLuint tex = 0;
        const std::int32_t material_index = primitive.m_material_index;
        if (!textures.empty() && material_index >= 0 &&
            static_cast<std::size_t>(material_index) < model.m_materials.size())
        {
            const std::int32_t texture_index = model.m_materials[material_index].m_texture_index;
            if (texture_index >= 0 &&
                static_cast<std::size_t>(texture_index) < textures.size())
            {
                tex = textures[texture_index];
                has_texture = true;
            }
        }
        if (has_texture)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex);
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
        }
        glBegin(GL_TRIANGLES);
        for (std::size_t i = 0; i + 2 < primitive.m_indices.size(); i += 3)
        {
            for (int k = 0; k < 3; ++k)
            {
                const std::uint32_t idx = primitive.m_indices[i + k];
                if (idx >= mesh.m_vertices.size()) continue;
                const auto& v = mesh.m_vertices[idx];
                glNormal3f(v.m_normal[0], v.m_normal[1], v.m_normal[2]);
                if (has_texture) glTexCoord2f(v.m_uv[0], v.m_uv[1]);
                glVertex3f(v.m_position[0], v.m_position[1], v.m_position[2]);
            }
        }
        glEnd();
        if (has_texture) glDisable(GL_TEXTURE_2D);
    }
    glEndList();
    return list;
}

void render_frame(s_draw_context& ctx)
{
    const s_camera& camera = *ctx.camera;
    const int drawable_width = ctx.drawable_width;
    const int drawable_height = ctx.drawable_height;
    SDL_Window* window = ctx.window;

    glViewport(0, 0, drawable_width, drawable_height);

    glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Apply camera.
    {
        const float aspect = static_cast<float>(drawable_width) /
            static_cast<float>(drawable_height > 0 ? drawable_height : 1);
        const float view_size = camera.m_distance;
        const float left = -aspect * view_size;
        const float right = aspect * view_size;
        const float bottom = -view_size;
        const float top = view_size;
        const float z_near = -1.0e6f;
        const float z_far = 1.0e6f;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(left, right, bottom, top, z_near, z_far);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(-camera.m_pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-camera.m_yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-camera.m_target[0], -camera.m_target[1], -camera.m_target[2]);
    }

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

        std::vector<std::array<float, 16>> world(node_count);
        std::vector<char> done(node_count, 0);
        std::function<std::array<float, 16>(std::size_t)> build =
            [&](std::size_t index) -> std::array<float, 16>
        {
            if (done[index] == 2) return world[index];
            if (done[index] == 1) throw std::runtime_error("hierarchy cycle");
            done[index] = 1;
            const auto local = local_xform[index];
            const auto& node = ctx.model->m_nodes[index];
            std::array<float, 16> result;
            if (node.m_parent >= 0)
            {
                const auto& parent_world = build(static_cast<std::size_t>(node.m_parent));
                result = multiply4(parent_world, local);
            }
            else
            {
                result = local;
            }
            world[index] = result;
            done[index] = 2;
            return result;
        };
        for (std::size_t index = 0; index < node_count; ++index)
        {
            build(index);
        }

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
                std::cout << "Skinned bbox: (" << skinned_min[0] << "," << skinned_min[1] << ","
                    << skinned_min[2] << ") to (" << skinned_max[0] << "," << skinned_max[1] << ","
                    << skinned_max[2] << ") extent=" << extent << " distance="
                    << ctx.camera->m_distance << '\n';
            }
        }

        // draw skinned mesh with per-material textures and UVs.
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glEnable(GL_LIGHT0);
        GLfloat ambient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        GLfloat light_pos[] = { 5.0f, 10.0f, 5.0f, 0.0f };
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
        glColor3f(1.0f, 1.0f, 1.0f);
        for (const auto& mesh : ctx.model->m_meshes)
        {
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
            if (ctx.mesh_visible && !(*ctx.mesh_visible)[
                static_cast<std::size_t>(&mesh - &ctx.model->m_meshes[0])]) continue;
            for (const auto& primitive : mesh.m_primitives)
            {
                if (primitive.m_indices.size() < 3) continue;
                const std::int32_t material_index = primitive.m_material_index;
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
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, tex);
                }
                else
                {
                    glDisable(GL_TEXTURE_2D);
                }
                glBegin(GL_TRIANGLES);
                for (std::size_t i = 0; i + 2 < primitive.m_indices.size(); i += 3)
                {
                    for (int k = 0; k < 3; ++k)
                    {
                        const std::uint32_t idx = primitive.m_indices[i + k];
                        if (idx >= mesh.m_vertices.size()) continue;
                        const auto& v = mesh.m_vertices[idx];
                        const auto sp = skin_position(v.m_position, v, mesh.m_rigid_node_index,
                            inverse_bind, world, ctx.model->m_nodes);
                        glNormal3f(v.m_normal[0], v.m_normal[1], v.m_normal[2]);
                        if (has_texture)
                        {
                            glTexCoord2f(v.m_uv[0], v.m_uv[1]);
                        }
                        glVertex3f(sp[0], sp[1], sp[2]);
                    }
                }
                glEnd();
                if (has_texture)
                {
                    glDisable(GL_TEXTURE_2D);
                }
            }
        }
        glDisable(GL_COLOR_MATERIAL);
    }

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
                    std::cout << "Stage bbox: (" << lo[0] << "," << lo[1] << "," << lo[2] << ") to ("
                        << hi[0] << "," << hi[1] << "," << hi[2] << ") distance="
                        << ctx.camera->m_distance << '\n';
                }
            }

            glEnable(GL_LIGHTING);
            glEnable(GL_NORMALIZE);
            glEnable(GL_LIGHT0);
            GLfloat ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
            GLfloat light_pos[] = { 0.3f, 1.0f, 0.5f, 0.0f };
            glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
            GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
            glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
            glColor3f(1.0f, 1.0f, 1.0f);
            const std::vector<std::vector<sdbz::mat4>>* stage_worlds = ctx.stage_worlds;
            const std::vector<std::unordered_map<std::string, std::vector<std::size_t>>>*
                meshes_by_names = ctx.stage_meshes_by_names;
            for (std::size_t ni = 0; ni < stage_nodes.size(); ++ni)
            {
                const auto& node = stage_nodes[ni];
                if (node.m_kind != 2 || !group_visible(ni)) continue;
                const std::unordered_map<std::string, std::vector<std::size_t>>* mbn =
                    meshes_by_names ? &(*meshes_by_names)[ctx.stage_index] : nullptr;
                if (!mbn) continue;
                auto it = mbn->find(node.m_name);
                if (it == mbn->end()) continue;
                glPushMatrix();
                gl_mult_matrix((*stage_worlds)[ctx.stage_index][ni]);
                for (const std::size_t mi : it->second)
                    glCallList((*ctx.stage_mesh_lists)[ctx.stage_index][mi]);
                glPopMatrix();
            }
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

    // ---- Image viewer ----
    else if (ctx.has_image && ctx.image && !ctx.has_textures)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, upload_texture(*ctx.image));

        const float scale = std::min(
            static_cast<float>(drawable_width) / static_cast<float>(ctx.image->m_width),
            static_cast<float>(drawable_height) / static_cast<float>(ctx.image->m_height));
        const float draw_width = static_cast<float>(ctx.image->m_width) * scale;
        const float draw_height = static_cast<float>(ctx.image->m_height) * scale;
        const float draw_x = (static_cast<float>(drawable_width) - draw_width) * 0.5f;
        const float draw_y = (static_cast<float>(drawable_height) - draw_height) * 0.5f;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, drawable_width, drawable_height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(draw_x, draw_y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(draw_x + draw_width, draw_y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(draw_x + draw_width, draw_y + draw_height);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(draw_x, draw_y + draw_height);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
    }

    // ---- HUD text ----
    if (ctx.has_font && ctx.font && ctx.glyph_textures)
    {
        begin_2d(drawable_width, drawable_height);

        std::string hud = "INFO.DAT/GAME.DAT viewer";
        if (ctx.has_model && ctx.model) hud += " | model: " + ctx.model->m_name;
        if (ctx.has_stage && ctx.stages && ctx.stage_index < ctx.stages->size())
            hud += " | stage: " + ctx.input_path.stem().string();
        if (ctx.has_motion && ctx.motion)
            hud += " | motion: " +
                ctx.motion->m_motions[ctx.motion_clip_index].m_name;
        else if (ctx.has_model) hud += " | motion: none";
        if (!ctx.mesh_filters.empty())
        {
            hud += " | meshes:";
            for (const auto& f : ctx.mesh_filters) hud += " " + f;
        }
        draw_text(hud, 10.0f, 10.0f, 1.0f, 255, 255, 0, *ctx.font, *ctx.glyph_textures);
        float line_y = 40.0f;
        if (ctx.part_groups)
        {
            for (std::size_t gi = 0; gi < ctx.part_groups->size() && gi < 9; ++gi)
            {
                const auto& group = (*ctx.part_groups)[gi];
                const bool hidden = group.m_active >= group.m_meshes.size();
                std::string line = std::to_string(gi + 1) + " " + group.m_label + ": " +
                    (hidden ? std::string("hidden")
                            : ctx.model->m_meshes[
                                group.m_meshes[group.m_active]].m_name) +
                    " [" + std::to_string(hidden ? 0 : group.m_active + 1) + "/" +
                    std::to_string(group.m_meshes.size()) + "]";
                if (hidden) draw_text(line, 10.0f, line_y, 0.8f, 140, 140, 140,
                    *ctx.font, *ctx.glyph_textures);
                else        draw_text(line, 10.0f, line_y, 0.8f, 255, 255, 255,
                    *ctx.font, *ctx.glyph_textures);
                line_y += 22.0f;
            }
        }
        if (ctx.has_stage)
        {
            std::string stage_line = std::string("E effect: ") + (ctx.show_effect ? "on" : "off") +
                " | C collision: " + (ctx.show_collision ? "on" : "off") +
                " | T templates: " + (ctx.show_templates ? "on" : "off") + " | F fit camera";
            draw_text(stage_line, 10.0f, line_y, 0.8f, 255, 255, 255,
                *ctx.font, *ctx.glyph_textures);
            line_y += 22.0f;
        }
        if (ctx.part_groups && !ctx.part_groups->empty())
        {
            draw_text("1-9 cycle parts | Shift reverse | 0 reset | F fit camera | drag orbit | right-drag pan",
                10.0f, line_y + 6.0f, 0.7f, 160, 200, 255,
                *ctx.font, *ctx.glyph_textures);
        }
        // Camera info in bottom right.
        {
            char cam_buf[128];
            std::snprintf(cam_buf, sizeof(cam_buf),
                "Cam: yaw=%.1f pitch=%.1f dist=%.1f target=(%.1f,%.1f,%.1f)",
                camera.m_yaw, camera.m_pitch, camera.m_distance,
                camera.m_target[0], camera.m_target[1], camera.m_target[2]);
            std::string cam_str(cam_buf);
            float cam_x = 10.0f;
            float cam_y = static_cast<float>(drawable_height) - 20.0f;
            draw_text(cam_str, cam_x, cam_y, 0.8f, 200, 200, 200,
                *ctx.font, *ctx.glyph_textures);
        }
        end_2d();
    }

    SDL_GL_SwapWindow(window);
    SDL_Delay(1);
}
