#include "interface/hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#include "rasterizer/gl_helpers.hpp"

void draw_hud(s_draw_context& ctx)
{
    const int drawable_width = ctx.drawable_width;
    const int drawable_height = ctx.drawable_height;
    const s_camera& camera = *ctx.camera;
    // ---- HUD text ----
    if (!ctx.debug.open && ctx.has_font && ctx.font && ctx.glyph_textures)
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
}

