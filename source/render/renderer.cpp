#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <fstream>
#include <thread>
#include <numbers>


#include "rasterizer/math_helpers.hpp"
#include "rasterizer/gl_helpers.hpp"
#include "render/model_renderer.hpp"
#include "render/stage_renderer.hpp"
#include "interface/hud.hpp"
#include "interface/vfs_overlay.hpp"
#include "rasterizer/c_rasterizer.hpp"

void render_frame(s_draw_context& ctx)
{
    const sdbz::s_camera& camera = *ctx.camera;
    const int drawable_width = ctx.drawable_width;
    const int drawable_height = ctx.drawable_height;
    c_window* window = ctx.window;

    c_rasterizer::viewport(0, 0, drawable_width, drawable_height);

    c_rasterizer::clear_color(0.06f, 0.08f, 0.12f, 1.0f);
    c_rasterizer::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Apply the active camera. The player camera uses a true perspective view
    // with a signed orbit distance; the world viewer remains orthographic.
    {
        const float aspect = static_cast<float>(drawable_width) /
            static_cast<float>(drawable_height > 0 ? drawable_height : 1);
        c_rasterizer::matrix_mode(GL_PROJECTION);
        c_rasterizer::load_identity();
        c_rasterizer::matrix_mode(GL_MODELVIEW);
        c_rasterizer::load_identity();
        if (camera.m_perspective)
        {
            const float near_plane = std::clamp(camera.m_near_plane, 0.01f, 10.0f);
            const float far_plane = std::max(camera.m_far_plane, near_plane + 1.0f);
            const float fov = std::clamp(camera.m_vertical_fov_degrees, 20.0f, 110.0f) *
                0.01745329251994329577f;
            const float top = near_plane * std::tan(fov * 0.5f);
            const float right = top * aspect;
            c_rasterizer::matrix_mode(GL_PROJECTION);
            c_rasterizer::frustum(-right, right, -top, top, near_plane, far_plane);
            c_rasterizer::matrix_mode(GL_MODELVIEW);

            const sdbz::s_camera_pose pose = sdbz::calculate_camera_pose(camera);
            const auto normalise = [](const std::array<float, 3>& value)
            {
                const float length = std::sqrt(value[0] * value[0] +
                    value[1] * value[1] + value[2] * value[2]);
                if (length <= 1.0e-6f)
                {
                    return std::array<float, 3>{ 0.0f, 0.0f, -1.0f };
                }
                return std::array<float, 3>{ value[0] / length,
                    value[1] / length, value[2] / length };
            };
            const std::array<float, 3> forward = normalise({
                pose.m_target[0] - pose.m_eye[0],
                pose.m_target[1] - pose.m_eye[1],
                pose.m_target[2] - pose.m_eye[2] });
            const auto cross = [](const std::array<float, 3>& a,
                const std::array<float, 3>& b)
            {
                return std::array<float, 3>{
                    a[1] * b[2] - a[2] * b[1],
                    a[2] * b[0] - a[0] * b[2],
                    a[0] * b[1] - a[1] * b[0] };
            };
            const auto dot = [](const std::array<float, 3>& a,
                const std::array<float, 3>& b)
            {
                return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
            };
            const std::array<float, 3> side = normalise(cross(forward, pose.m_up));
            const std::array<float, 3> up = cross(side, forward);
            const std::array<GLfloat, 16> view_matrix{
                side[0], up[0], -forward[0], 0.0f,
                side[1], up[1], -forward[1], 0.0f,
                side[2], up[2], -forward[2], 0.0f,
                -dot(side, pose.m_eye), -dot(up, pose.m_eye),
                dot(forward, pose.m_eye), 1.0f
            };
            c_rasterizer::mult_matrix(view_matrix.data());
        }
        else
        {
            const float view_size = std::max(camera.m_distance, 0.001f);
            const float left = -aspect * view_size;
            const float right = aspect * view_size;
            const float bottom = -view_size;
            const float top = view_size;
            c_rasterizer::matrix_mode(GL_PROJECTION);
            c_rasterizer::ortho(left, right, bottom, top, -1.0e6f, 1.0e6f);
            c_rasterizer::matrix_mode(GL_MODELVIEW);
            c_rasterizer::rotate(-camera.m_pitch, 1.0f, 0.0f, 0.0f);
            c_rasterizer::rotate(-camera.m_yaw, 0.0f, 1.0f, 0.0f);
            c_rasterizer::translate(-camera.m_target[0], -camera.m_target[1], -camera.m_target[2]);
        }
    }

    render_model_viewer(ctx);
    render_stage_viewer(ctx);

    // ---- Image viewer ----
    if (ctx.has_image && ctx.image && !ctx.has_textures)
    {
        c_rasterizer::enable(GL_TEXTURE_2D);
        c_rasterizer::bind_texture(GL_TEXTURE_2D, upload_texture(*ctx.image));

        const float scale = std::min(
            static_cast<float>(drawable_width) / static_cast<float>(ctx.image->width),
            static_cast<float>(drawable_height) / static_cast<float>(ctx.image->height));
        const float draw_width = static_cast<float>(ctx.image->width) * scale;
        const float draw_height = static_cast<float>(ctx.image->height) * scale;
        const float draw_x = (static_cast<float>(drawable_width) - draw_width) * 0.5f;
        const float draw_y = (static_cast<float>(drawable_height) - draw_height) * 0.5f;

        c_rasterizer::matrix_mode(GL_PROJECTION);
        c_rasterizer::load_identity();
        c_rasterizer::ortho(0, drawable_width, drawable_height, 0, -1, 1);
        c_rasterizer::matrix_mode(GL_MODELVIEW);
        c_rasterizer::load_identity();

        c_rasterizer::color3f(1.0f, 1.0f, 1.0f);
        c_rasterizer::begin(GL_QUADS);
        c_rasterizer::tex_coord2f(0.0f, 0.0f); c_rasterizer::vertex2f(draw_x, draw_y);
        c_rasterizer::tex_coord2f(1.0f, 0.0f); c_rasterizer::vertex2f(draw_x + draw_width, draw_y);
        c_rasterizer::tex_coord2f(1.0f, 1.0f); c_rasterizer::vertex2f(draw_x + draw_width, draw_y + draw_height);
        c_rasterizer::tex_coord2f(0.0f, 1.0f); c_rasterizer::vertex2f(draw_x, draw_y + draw_height);
        c_rasterizer::end();
        c_rasterizer::disable(GL_TEXTURE_2D);
        c_rasterizer::disable(GL_LIGHTING);
    }

    draw_hud(ctx);

    // Debug menu overlay (drawn on top of the scene/HUD).
    if (ctx.debug.open && ctx.has_font && ctx.font && ctx.glyph_textures)
    {
        debug_menu_draw(ctx.debug, ctx.drawable_width, ctx.drawable_height,
            *ctx.font, *ctx.glyph_textures);
    }

    draw_vfs_overlay(ctx);

    if (window)
    {
        window->swap_buffers();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

