#include "rasterizer/gl_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "assets/assets.hpp"

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
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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