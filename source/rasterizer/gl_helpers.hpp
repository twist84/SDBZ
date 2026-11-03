#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL_opengl.h>

#include "assets/assets.hpp"
#include "core/model.hpp"
#include "core/math.hpp"

void upload_glyph_textures(const s_ffon_font& font, std::vector<GLuint>& glyph_textures);
void begin_2d(int width, int height);
void end_2d();
void draw_glyph(GLuint tex, float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b);
void draw_text(const std::string& text, float x, float y, float scale,
    std::uint8_t r, std::uint8_t g, std::uint8_t b,
    const s_ffon_font& font, const std::vector<GLuint>& glyph_textures);
void gl_mult_matrix(const sdbz::mat4& m);
GLuint compile_static_mesh_list(const sdbz::s_model& model,
    const sdbz::s_model_mesh& mesh,
    const std::vector<GLuint>& textures);