#include "interface/vfs_overlay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#include "rasterizer/gl_helpers.hpp"

void draw_vfs_overlay(s_draw_context& ctx)
{
    // Virtual file system browser overlay over an INFO.DAT/GAME.DAT archive.
    if (ctx.vfs_open && ctx.has_font && ctx.font && ctx.glyph_textures &&
        ctx.vfs_archive && !ctx.vfs_entries.empty())
    {
        begin_2d(ctx.drawable_width, ctx.drawable_height);
        draw_text("ARCHIVE VFS  (INFO.DAT/GAME.DAT)  (Esc: close)", 10.0f, 10.0f,
            1.0f, 255, 255, 0, *ctx.font, *ctx.glyph_textures);
        // Breadcrumb path.
        std::string breadcrumb;
        for (std::size_t i = 0; i < ctx.vfs_path_components.size(); ++i)
        {
            if (i != 0) breadcrumb += " > ";
            breadcrumb += ctx.vfs_path_components[i];
        }
        if (breadcrumb.empty()) breadcrumb = "(root)";
        draw_text("Path: " + breadcrumb, 10.0f, 32.0f, 0.7f, 200, 200, 200,
            *ctx.font, *ctx.glyph_textures);
        float y = 54.0f;
        int max_entries = (ctx.drawable_height - 126) / 18;
        if (max_entries < 1) max_entries = 1;
        int start = 0;
        if (ctx.vfs_selected >= start + max_entries)
            start = ctx.vfs_selected - max_entries + 1;
        if (ctx.vfs_selected < start)
            start = ctx.vfs_selected;
        if (start < 0) start = 0;
        if (start + max_entries > static_cast<int>(ctx.vfs_entries.size()))
            start = static_cast<int>(ctx.vfs_entries.size()) - max_entries;
        if (start < 0) start = 0;
        for (int i = start; i < start + max_entries && i < static_cast<int>(ctx.vfs_entries.size()); ++i)
        {
            const auto& ve = ctx.vfs_entries[i];
            const bool selected = (i == ctx.vfs_selected);
            const std::uint8_t r = selected ? 255 : 200;
            const std::uint8_t g = selected ? 255 : 200;
            const std::uint8_t b = selected ? 100 : 200;
            std::string line = (selected ? "> " : "  ");
            line += (ve.is_dir ? "[dir] " : "[file]") + ve.name;
            draw_text(line, 10.0f, y, 0.7f, r, g, b, *ctx.font, *ctx.glyph_textures);
            y += 18.0f;
        }
        draw_text("Enter: enter dir / load file  |  Backspace: up  |  Esc: close",
            10.0f, static_cast<float>(ctx.drawable_height) - 24.0f, 0.6f, 160, 160, 160,
            *ctx.font, *ctx.glyph_textures);
        end_2d();
    }
}

