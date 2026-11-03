#include "rasterizer/gl_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "assets/assets.hpp"
#include "rasterizer/c_rasterizer.hpp"

void upload_glyph_textures(const s_ffon_font& font, std::vector<GLuint>& glyph_textures)
{
    glyph_textures.resize(font.glyph_count);
    for (std::uint32_t gi = 0; gi < font.glyph_count; ++gi)
    {
        const auto& data = font.glyphs[gi];
        const std::uint8_t width = font.width;
        const std::uint8_t height = font.height;
        const std::uint8_t row_stride = font.bytes_per_row;

        GLuint tex = 0;
        c_rasterizer::gen_textures(1, &tex);
        c_rasterizer::bind_texture(GL_TEXTURE_2D, tex);
        c_rasterizer::tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        c_rasterizer::tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        c_rasterizer::tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        c_rasterizer::tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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
                rgba[base + 3] = intensity;
            }
        }
        c_rasterizer::tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, rgba.data());
        glyph_textures[gi] = tex;
    }
}

void begin_2d(int width, int height)
{
    c_rasterizer::matrix_mode(GL_PROJECTION);
    c_rasterizer::push_matrix();
    c_rasterizer::load_identity();
    c_rasterizer::ortho(0.0, width, height, 0.0, -1.0, 1.0);
    c_rasterizer::matrix_mode(GL_MODELVIEW);
    c_rasterizer::push_matrix();
    c_rasterizer::load_identity();
    c_rasterizer::disable(GL_DEPTH_TEST);
    c_rasterizer::disable(GL_LIGHTING);
    c_rasterizer::enable(GL_BLEND);
    c_rasterizer::blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    c_rasterizer::tex_envi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void end_2d()
{
    c_rasterizer::disable(GL_BLEND);
    c_rasterizer::enable(GL_DEPTH_TEST);
    c_rasterizer::matrix_mode(GL_PROJECTION);
    c_rasterizer::pop_matrix();
    c_rasterizer::matrix_mode(GL_MODELVIEW);
    c_rasterizer::pop_matrix();
}

void draw_glyph(GLuint tex, float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    c_rasterizer::enable(GL_TEXTURE_2D);
    c_rasterizer::bind_texture(GL_TEXTURE_2D, tex);
    c_rasterizer::color3f(r / 255.0f, g / 255.0f, b / 255.0f);
    c_rasterizer::begin(GL_QUADS);
    c_rasterizer::tex_coord2f(0.0f, 0.0f); c_rasterizer::vertex2f(x, y);
    c_rasterizer::tex_coord2f(1.0f, 0.0f); c_rasterizer::vertex2f(x + w, y);
    c_rasterizer::tex_coord2f(1.0f, 1.0f); c_rasterizer::vertex2f(x + w, y + h);
    c_rasterizer::tex_coord2f(0.0f, 1.0f); c_rasterizer::vertex2f(x, y + h);
    c_rasterizer::end();
    c_rasterizer::disable(GL_TEXTURE_2D);
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
    c_rasterizer::mult_matrix(column_major);
}

GLuint compile_static_mesh_list(const sdbz::s_model& model,
    const sdbz::s_model_mesh& mesh,
    const std::vector<GLuint>& textures)
{
    const GLuint list = c_rasterizer::gen_lists(1);
    c_rasterizer::new_list(list, GL_COMPILE);
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
            c_rasterizer::enable(GL_TEXTURE_2D);
            c_rasterizer::bind_texture(GL_TEXTURE_2D, tex);
        }
        else
        {
            c_rasterizer::disable(GL_TEXTURE_2D);
        }
        c_rasterizer::begin(GL_TRIANGLES);
        for (std::size_t i = 0; i + 2 < primitive.m_indices.size(); i += 3)
        {
            for (int k = 0; k < 3; ++k)
            {
                const std::uint32_t idx = primitive.m_indices[i + k];
                if (idx >= mesh.m_vertices.size()) continue;
                const auto& v = mesh.m_vertices[idx];
                c_rasterizer::normal3f(v.m_normal[0], v.m_normal[1], v.m_normal[2]);
                if (has_texture) c_rasterizer::tex_coord2f(v.m_uv[0], v.m_uv[1]);
                c_rasterizer::vertex3f(v.m_position[0], v.m_position[1], v.m_position[2]);
            }
        }
        c_rasterizer::end();
        if (has_texture) c_rasterizer::disable(GL_TEXTURE_2D);
    }
    c_rasterizer::end_list();
    return list;
}
