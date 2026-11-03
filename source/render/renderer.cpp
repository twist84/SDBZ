#include "render/renderer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>


#include "rasterizer/math_helpers.hpp"
#include "rasterizer/gl_helpers.hpp"
#include "render/model_renderer.hpp"
#include "render/stage_renderer.hpp"
#include "interface/hud.hpp"
#include "interface/vfs_overlay.hpp"

void render_frame(s_draw_context& ctx)
{
    const s_camera& camera = *ctx.camera;
    const int drawable_width = ctx.drawable_width;
    const int drawable_height = ctx.drawable_height;
    c_window* window = ctx.window;

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

    render_model_viewer(ctx);
    render_stage_viewer(ctx);

    // ---- Image viewer ----
    if (ctx.has_image && ctx.image && !ctx.has_textures)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, upload_texture(*ctx.image));

        const float scale = std::min(
            static_cast<float>(drawable_width) / static_cast<float>(ctx.image->width),
            static_cast<float>(drawable_height) / static_cast<float>(ctx.image->height));
        const float draw_width = static_cast<float>(ctx.image->width) * scale;
        const float draw_height = static_cast<float>(ctx.image->height) * scale;
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

