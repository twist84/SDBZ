#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "core/animation.hpp"
#include "core/binary_reader.hpp"
#include "core/container.hpp"
#include "core/model.hpp"
#include "core/stage.hpp"
#include "core/texture.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    constexpr int k_logical_width = 1120;
    constexpr int k_logical_height = 490;

    struct s_input_state
    {
        bool m_move_left = false;
        bool m_move_right = false;
        bool m_jump = false;
        bool m_attack = false;
    };

    void update_key(s_input_state& input, const SDL_Keycode key,
        const bool pressed)
    {
        switch (key)
        {
        case SDLK_A:
        case SDLK_LEFT:
            input.m_move_left = pressed;
            break;
        case SDLK_D:
        case SDLK_RIGHT:
            input.m_move_right = pressed;
            break;
        case SDLK_SPACE:
        case SDLK_UP:
            input.m_jump = pressed;
            break;
        case SDLK_J:
            input.m_attack = pressed;
            break;
        default:
            break;
        }
    }

    void update_gamepad_button(s_input_state& input,
        const SDL_GamepadButton button, const bool pressed)
    {
        switch (button)
        {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            input.m_move_left = pressed;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            input.m_move_right = pressed;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
        case SDL_GAMEPAD_BUTTON_SOUTH:
            input.m_jump = pressed;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            input.m_attack = pressed;
            break;
        default:
            break;
        }
    }

    // Upload an s_texture_image to OpenGL and return the texture name.
    GLuint upload_texture(const sdbz::s_texture_image& image)
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(image.m_width),
            static_cast<GLsizei>(image.m_height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
            image.m_rgba.data());
        return tex;
    }


// FFON font loader (see research/FFON/5-24-2_FFON_format.md)
struct s_ffon_font {
    std::uint8_t width = 24;
    std::uint8_t height = 24;
    std::uint8_t bytes_per_row = 12;
    std::uint32_t glyph_count = 0;
    std::uint32_t bytes_per_glyph = 0;
    std::vector<std::uint16_t> char_map;     // char_code -> glyph_id (0 = none)
    std::vector<std::vector<std::uint8_t>> glyphs; // glyph_id -> raw 4bpp pixels
};

bool load_ffon(const std::filesystem::path& path, s_ffon_font& font)
{
    auto data = sdbz::read_file(path);
    if (data.size() < 0x40 || std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "FFON")
    {
        std::cerr << "Not an FFON file: " << path << '\n';
        return false;
    }

    // Header fields
    const auto page_count  = sdbz::read_u32_le(std::span(data.data() + 0x10, 4), 0).value();
    const auto last_column = sdbz::read_u32_le(std::span(data.data() + 0x14, 4), 0).value();
    const auto glyph_count = sdbz::read_u32_le(std::span(data.data() + 0x18, 4), 0).value();
    const auto width  = data[0x1C];
    const auto height = data[0x1D];
    const auto row_stride = data[0x1F];

    const auto columns_per_page = last_column + 1;
    const auto char_map_count = page_count * columns_per_page;
    const auto glyph_data_offset = 0xA0 + char_map_count * 2;

    font.width = width;
    font.height = height;
    font.bytes_per_row = row_stride;
    font.glyph_count = glyph_count;
    font.bytes_per_glyph = row_stride * height;

    // Parse char map: slot -> glyph_id (1-based, 0 = none)
    font.char_map.assign(char_map_count, 0);
    for (std::uint32_t slot = 0; slot < char_map_count; ++slot)
    {
        const auto glyph_id = sdbz::read_u16_le(std::span(data.data() + 0xA0 + slot * 2, 2), 0).value();
        font.char_map[slot] = glyph_id;
    }

    // Read glyph bitmaps
    font.glyphs.resize(glyph_count);
    for (std::uint32_t gi = 0; gi < glyph_count; ++gi)
    {
        const auto offset = glyph_data_offset + gi * font.bytes_per_glyph;
        font.glyphs[gi].resize(font.bytes_per_glyph);
        std::memcpy(font.glyphs[gi].data(), data.data() + offset, font.bytes_per_glyph);
    }

    std::cout << "Loaded FFON font: " << path.filename().string()
        << " (" << glyph_count << " glyphs, " << width << "x" << height << ")\n";
    return true;
}

// Render a single glyph cell into an OpenGL texture
GLuint upload_glyph_texture(const std::vector<std::uint8_t>& data,
    std::uint8_t width, std::uint8_t height, std::uint8_t row_stride)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Expand 4bpp to RGBA
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    return tex;
}

// Horizontal ink bounds [left, right) of a glyph in pixels; (0,0) if blank.
std::pair<int, int> glyph_ink_bounds(const s_ffon_font& font, const std::vector<std::uint8_t>& data)
{
    int left = font.width;
    int right = 0;
    for (std::uint32_t y = 0; y < font.height; ++y)
    {
        for (std::uint32_t x = 0; x < font.width; ++x)
        {
            const auto packed = data[y * font.bytes_per_row + (x / 2)];
            const auto value = (x & 1) ? (packed >> 4) : (packed & 0x0F);
            if (value != 0)
            {
                left = std::min(left, static_cast<int>(x));
                right = std::max(right, static_cast<int>(x) + 1);
            }
        }
    }
    if (right == 0) return { 0, 0 };
    return { left, right };
}

// Switch to a top-left pixel-space overlay (the 3D camera uses its own ortho/modelview).
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

// Draw a single glyph as a textured quad (call between begin_2d/end_2d)
void draw_glyph(GLuint tex, float x, float y, float w, float h, std::uint8_t r, std::uint8_t g, std::uint8_t b)
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

// Draw ASCII text using FFON glyphs (pixel coordinates, origin top-left).
// Spacing matches arika_ffon.py render_text: glyphs trimmed to their ink box,
// 3 px between glyphs, 8 px for a space.
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
            font.width * scale, font.height * scale, r, g, b);
        cursor_x += ((right - left) + k_spacing) * scale;
    }
}

// Load every XPIX texture chunk from an FPIX container into OpenGL textures,
        // returning texture names indexed by texture slot.
        std::vector<GLuint> load_fpix_textures(const std::vector<std::uint8_t>& data)
    {
        std::vector<GLuint> textures;
        if (data.size() < 32 || std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "FPIX")
        {
            return textures;
        }
        const auto directory = sdbz::parse_chunked_container(data, "FPIX");
        for (const auto& chunk : directory.m_chunks)
        {
            if (chunk.magic_string() != "XPIX")
            {
                continue;
            }
            const std::span<const std::uint8_t> chunk_data(data.data() + chunk.m_offset, chunk.m_size);
            try
            {
                const auto image = sdbz::decode_xpix(chunk_data);
                textures.push_back(upload_texture(image));
                std::cout << "Loaded texture " << textures.size() << " from XPIX chunk "
                    << chunk.m_offset << " (" << image.m_width << "x" << image.m_height << ")\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "XPIX decode failed: " << exception.what() << '\n';
            }
        }
        return textures;
    }

    // Camera state for orbiting the model.
    struct s_camera
    {
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
        float m_distance = 6.0f;
        std::array<float, 3> m_target{ 0.0f, 0.0f, 0.0f };
    };

    void apply_camera(const s_camera& camera, int drawable_width, int drawable_height)
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
}

// ---- Asset discovery: given a model, find its texture, motions and the game font ----
namespace fs = std::filesystem;

std::string lower_copy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool has_extension(const fs::path& path, const char* extension)
{
    return lower_copy(path.extension().string()) == extension;
}

fs::path directory_of(const fs::path& path)
{
    const auto parent = path.parent_path();
    return parent.empty() ? fs::path(".") : parent;
}

// Regular files directly inside `dir` with the given (lowercase) extension, sorted by name.
std::vector<fs::path> list_files(const fs::path& dir, const char* extension)
{
    std::vector<fs::path> result;
    std::error_code ec;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec))
    {
        std::error_code entry_ec;
        if (it->is_regular_file(entry_ec) && has_extension(it->path(), extension))
        {
            result.push_back(it->path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

// "p01a" -> "p01": drop trailing variant letters that follow a digit.
std::string character_prefix(const std::string& stem)
{
    std::string prefix = lower_copy(stem);
    std::size_t end = prefix.size();
    while (end > 0 && std::isalpha(static_cast<unsigned char>(prefix[end - 1]))) --end;
    if (end > 0 && end < prefix.size() && std::isdigit(static_cast<unsigned char>(prefix[end - 1])))
    {
        prefix.erase(end);
    }
    return prefix;
}

// p01a.mdl -> p01a.pix beside it (case-insensitive), else any .pix sharing the character prefix.
fs::path find_texture_for(const fs::path& model_path)
{
    const auto candidates = list_files(directory_of(model_path), ".pix");
    const std::string stem = lower_copy(model_path.stem().string());
    for (const auto& candidate : candidates)
    {
        if (lower_copy(candidate.stem().string()) == stem) return candidate;
    }
    const std::string prefix = character_prefix(stem);
    for (const auto& candidate : candidates)
    {
        if (lower_copy(candidate.stem().string()).rfind(prefix, 0) == 0) return candidate;
    }
    return {};
}

// Motion packages (.pmt) beside the model: those sharing the character prefix
// (p01a.mdl -> p01sel.pmt, p01act.pmt), or every .pmt in the folder if none match.
std::vector<fs::path> find_motions_for(const fs::path& model_path)
{
    const auto all = list_files(directory_of(model_path), ".pmt");
    const std::string prefix = character_prefix(model_path.stem().string());
    std::vector<fs::path> matching;
    for (const auto& candidate : all)
    {
        if (lower_copy(candidate.stem().string()).rfind(prefix, 0) == 0) matching.push_back(candidate);
    }
    return matching.empty() ? all : matching;
}

// Look for a .fon near the model: its folder and a few parents, a few levels deep each.
fs::path find_font_near(const fs::path& start)
{
    std::error_code ec;
    std::vector<fs::path> roots;
    fs::path dir = fs::absolute(start, ec);
    for (int level = 0; level < 4 && !dir.empty(); ++level)
    {
        roots.push_back(dir);
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    roots.push_back(fs::current_path(ec));
    for (const auto& root : roots)
    {
        std::error_code scan_ec;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, scan_ec), end;
             !scan_ec && it != end; it.increment(scan_ec))
        {
            if (it.depth() >= 3) it.disable_recursion_pending();
            std::error_code entry_ec;
            if (it->is_regular_file(entry_ec) && has_extension(it->path(), ".fon")) return it->path();
        }
    }
    return {};
}

// ---- Small row-major 4x4 helpers (translation in [3], [7], [11]) ----
using mat4 = std::array<float, 16>;

mat4 mat4_mul(const mat4& a, const mat4& b)
{
    mat4 out{};
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

std::array<float, 3> mat4_point(const mat4& m, const std::array<float, 3>& p)
{
    return {
        m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3],
        m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7],
        m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]
    };
}

// Local transform T * Rz * Ry * Rx * S with Euler angles in radians (same convention as FMDL nodes).
mat4 trs_matrix(const std::array<float, 3>& t, const std::array<float, 3>& euler, const std::array<float, 3>& s)
{
    const float cx = std::cos(euler[0]), sx = std::sin(euler[0]);
    const float cy = std::cos(euler[1]), sy = std::sin(euler[1]);
    const float cz = std::cos(euler[2]), sz = std::sin(euler[2]);
    mat4 m{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    m[0] = cz * cy * s[0];
    m[1] = (-sz * cx + cz * sy * sx) * s[1];
    m[2] = (sz * sx + cz * sy * cx) * s[2];
    m[4] = sz * cy * s[0];
    m[5] = (cz * cx + sz * sy * sx) * s[1];
    m[6] = (-cz * sx + sz * sy * cx) * s[2];
    m[8] = -sy * s[0];
    m[9] = cy * sx * s[1];
    m[10] = cy * cx * s[2];
    m[3] = t[0];
    m[7] = t[1];
    m[11] = t[2];
    return m;
}

// Multiply the current OpenGL matrix by a row-major mat4.
void gl_mult_matrix(const mat4& m)
{
    const GLfloat column_major[16] = {
        m[0], m[4], m[8],  m[12],
        m[1], m[5], m[9],  m[13],
        m[2], m[6], m[10], m[14],
        m[3], m[7], m[11], m[15] };
    glMultMatrixf(column_major);
}

// Compile one rigid mesh (all of its material primitives) into a display list.
GLuint compile_static_mesh_list(const sdbz::s_model& model, const sdbz::s_model_mesh& mesh,
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
            if (texture_index >= 0 && static_cast<std::size_t>(texture_index) < textures.size())
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

// ---- Mesh part groups: each number key cycles the variants of one body part ----
struct s_part_group
{
    std::string m_label;                // e.g. "Head", "Hair", "LHand"
    std::vector<std::size_t> m_meshes;  // mesh indices, one per variant
    std::size_t m_active = 0;           // index into m_meshes; == m_meshes.size() means hidden
};

// "MeshLHandA" -> "LHand", "ShapeHeadAB" -> "Head", "MeshHair" -> "Hair".
// A trailing run of capitals/digits after a lowercase letter is a variant suffix
// (a lone trailing L/R is kept, since that means left/right rather than variant).
std::string part_base_name(const std::string& name)
{
    std::string base = name;
    for (const char* prefix : { "Mesh", "Shape" })
    {
        const std::size_t len = std::strlen(prefix);
        if (base.size() > len && base.compare(0, len, prefix) == 0)
        {
            base.erase(0, len);
            break;
        }
    }
    std::size_t end = base.size();
    while (end > 1 && (std::isupper(static_cast<unsigned char>(base[end - 1])) ||
                       std::isdigit(static_cast<unsigned char>(base[end - 1]))))
    {
        --end;
    }
    if (end > 0 && end < base.size() && std::islower(static_cast<unsigned char>(base[end - 1])))
    {
        const std::string suffix = base.substr(end);
        if (suffix != "L" && suffix != "R") base.erase(end);
    }
    return base.empty() ? name : base;
}

void apply_part_visibility(const std::vector<s_part_group>& groups, std::vector<bool>& visible)
{
    std::fill(visible.begin(), visible.end(), false);
    for (const auto& group : groups)
    {
        for (std::size_t i = 0; i < group.m_meshes.size(); ++i)
        {
            visible[group.m_meshes[i]] = (i == group.m_active);
        }
    }
}

int main(int argc, char** argv)
{
    const std::filesystem::path input_path = argc > 1 ? argv[1] : "assets/extracted";
    std::cout << "Input: " << input_path.string() << '\n';

    bool is_pix = false;
    bool is_mdl = false;
    bool is_hit = false;
    sdbz::s_texture_image loaded_image;
    bool has_loaded_image = false;
    sdbz::s_model loaded_model;
    bool has_loaded_model = false;
    sdbz::s_motion_package loaded_motion;
    bool has_loaded_motion = false;
    float animation_time = 0.0f;
    std::size_t motion_clip_index = 0;
    sdbz::s_hit loaded_hit;
    bool has_loaded_hit = false;
    std::vector<GLuint> texture_names;
    bool has_textures = false;
    std::vector<std::string> mesh_filters;
    std::vector<bool> mesh_visible;
    std::vector<s_part_group> part_groups;

    // Stage scene (.stg places meshes from the sibling .mdl; .hit is collision)
    std::vector<sdbz::s_stage> loaded_stages;
    std::size_t loaded_stage_index = 0;
    bool has_loaded_stage = false;
    bool is_stg = false;
    std::vector<sdbz::s_model> stage_models;
    std::vector<bool> stage_models_loaded;
    std::vector<sdbz::s_hit> stage_hits;
    std::vector<bool> stage_hits_loaded;
    std::vector<std::vector<GLuint>> stage_textures;
    std::vector<bool> stage_textures_loaded;
    std::vector<std::filesystem::path> stage_pix_paths;
    std::vector<std::vector<mat4>> stage_worlds;
    std::vector<std::vector<char>> stage_groups;
    std::vector<std::unordered_map<std::string, std::vector<std::size_t>>> stage_meshes_by_names;
    std::vector<std::unordered_map<std::string, std::size_t>> stage_hit_by_names;
    std::vector<std::vector<GLuint>> stage_mesh_lists;           // compiled lazily once GL + textures exist
    bool show_effect = false;
    bool show_templates = false;
    bool show_collision = false;
    bool has_stage_model = false;
    bool has_stage_hit = false;

    // FFON font
    s_ffon_font font;
    bool has_font = false;
    std::vector<GLuint> glyph_textures;

    // Extra arguments after the model are classified by extension, in any order:
    //   *.pmt / *.mot -> motion, *.pix -> texture, *.fon -> font, --mesh NAME -> mesh filter.
    // Anything not given is looked up automatically next to the model.
    std::vector<fs::path> motion_paths;
    fs::path texture_path;
    fs::path font_path;
    bool explicit_motion = false;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--mesh" && i + 1 < argc)
        {
            mesh_filters.push_back(argv[++i]);
            continue;
        }
        const fs::path path = arg;
        if (has_extension(path, ".pmt") || has_extension(path, ".mot"))
        {
            motion_paths.push_back(path);
            explicit_motion = true;
        }
        else if (has_extension(path, ".pix")) texture_path = path;
        else if (has_extension(path, ".fon")) font_path = path;
        else std::cerr << "Ignoring unrecognised argument: " << arg << '\n';
    }

    if (argc > 1 && std::filesystem::is_regular_file(input_path))
    {
        const std::string ext = input_path.extension().string();
        if (ext == ".pix" || ext == ".PIX")
        {
            is_pix = true;
            try
            {
                const auto file_data = sdbz::read_file(input_path);
                loaded_image = sdbz::decode_first_fpix_texture(file_data);
                has_loaded_image = true;
                std::cout << "Loaded texture: " << loaded_image.m_width << "x"
                    << loaded_image.m_height << '\n';
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Texture preview unavailable: " << exception.what() << '\n';
                return 2;
            }
        }
        else if (ext == ".mdl" || ext == ".MDL")
        {
            is_mdl = true;
            try
            {
                const auto file_data = sdbz::read_file(input_path);
                loaded_model = sdbz::parse_fmdl(file_data, input_path.stem().string());
                has_loaded_model = true;
                std::cout << "Loaded model: " << loaded_model.m_name
                    << " (meshes=" << loaded_model.m_meshes.size()
                    << " materials=" << loaded_model.m_materials.size()
                    << " nodes=" << loaded_model.m_nodes.size() << ")\n";
                // Group meshes into parts (head, hair, hands...) and show the first variant of each.
                mesh_visible.assign(loaded_model.m_meshes.size(), false);
                for (std::size_t mi = 0; mi < loaded_model.m_meshes.size(); ++mi)
                {
                    const auto& mesh_name = loaded_model.m_meshes[mi].m_name;
                    if (!mesh_filters.empty())
                    {
                        bool matches = false;
                        for (const auto& f : mesh_filters)
                        {
                            if (mesh_name.find(f) != std::string::npos) { matches = true; break; }
                        }
                        if (!matches) continue;
                    }
                    const std::string label = part_base_name(mesh_name);
                    auto it = std::find_if(part_groups.begin(), part_groups.end(),
                        [&](const s_part_group& g) { return g.m_label == label; });
                    if (it == part_groups.end())
                    {
                        part_groups.push_back({ label, {}, 0 });
                        it = part_groups.end() - 1;
                    }
                    it->m_meshes.push_back(mi);
                }
                apply_part_visibility(part_groups, mesh_visible);
                for (std::size_t gi = 0; gi < part_groups.size(); ++gi)
                {
                    std::cout << (gi < 9 ? std::to_string(gi + 1) : std::string("-")) << ": "
                        << part_groups[gi].m_label << " (";
                    for (std::size_t vi = 0; vi < part_groups[gi].m_meshes.size(); ++vi)
                    {
                        std::cout << (vi ? ", " : "")
                            << loaded_model.m_meshes[part_groups[gi].m_meshes[vi]].m_name;
                    }
                    std::cout << ")\n";
                }
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Model load unavailable: " << exception.what() << '\n';
                return 2;
            }
        }
        else if (ext == ".hit" || ext == ".HIT")
        {
            is_hit = true;
            try
            {
                const auto file_data = sdbz::read_file(input_path);
                loaded_hit = sdbz::parse_hit(file_data);
                has_loaded_hit = true;
                std::cout << "Loaded hit: " << loaded_hit.m_objects.size() << " objects, "
                    << loaded_hit.m_polygons.size() << " polys, "
                    << loaded_hit.m_vertices.size() << " verts\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Hit load unavailable: " << exception.what() << '\n';
                return 2;
            }
        }
        else if (has_extension(input_path, ".stg"))
        {
            is_stg = true;
            try
            {
                // Load ALL .stg files in the same folder (e.g. s03a.stg, s03b.stg)
                std::vector<fs::path> stage_files = list_files(directory_of(input_path), ".stg");
                std::sort(stage_files.begin(), stage_files.end());
                for (const auto& stg_path : stage_files)
                {
                    try
                    {
                        sdbz::s_stage stg = sdbz::parse_stg(sdbz::read_file(stg_path));
                        loaded_stages.push_back(std::move(stg));
                        stage_models.emplace_back();
                        stage_models_loaded.push_back(false);
                        stage_hits.emplace_back();
                        stage_hits_loaded.push_back(false);
                        stage_textures.emplace_back();
                        stage_textures_loaded.push_back(false);
                        stage_pix_paths.emplace_back();
                        stage_worlds.emplace_back();
                        stage_groups.emplace_back();
                        stage_meshes_by_names.emplace_back();
                        stage_hit_by_names.emplace_back();
                        stage_mesh_lists.emplace_back();
                        std::cout << "Loaded stage: " << stg_path.filename().string() << " (" << loaded_stages.back().m_nodes.size() << " nodes)\n";
                    }
                    catch (const std::exception& exception)
                    {
                        std::cerr << "Stage load failed for " << stg_path.filename().string() << ": " << exception.what() << '\n';
                    }
                }
                if (!loaded_stages.empty())
                {
                    has_loaded_stage = true;
                    loaded_stage_index = 0;
                    // Load companions for each stage
                    for (std::size_t si = 0; si < loaded_stages.size(); ++si)
                    {
                        const auto& stg_path = stage_files[si];
                        const auto sibling = [&](const char* extension) -> fs::path
                        {
                            const std::string stem = lower_copy(stg_path.stem().string());
                            for (const auto& candidate : list_files(directory_of(stg_path), extension))
                            {
                                if (lower_copy(candidate.stem().string()) == stem) return candidate;
                            }
                            return {};
                        };
                        // Stage model (.mdl)
                        const fs::path model_file = sibling(".mdl");
                        if (!model_file.empty())
                        {
                            try
                            {
                                stage_models[si] = sdbz::parse_fmdl(sdbz::read_file(model_file), model_file.stem().string());
                                stage_models_loaded[si] = true;
                                has_stage_model = true;
                                std::cout << "Stage model " << model_file.filename().string() << ": "
                                    << stage_models[si].m_meshes.size() << " meshes\n";
                                for (std::size_t mi = 0; mi < stage_models[si].m_meshes.size(); ++mi)
                                {
                                    stage_meshes_by_names[si][stage_models[si].m_meshes[mi].m_name].push_back(mi);
                                }
                            }
                            catch (const std::exception& exception)
                            {
                                std::cerr << "Stage model unavailable: " << exception.what() << '\n';
                            }
                        }
                        else
                        {
                            std::cerr << "No " << stg_path.stem().string() << ".mdl next to the stage; showing the node hierarchy only\n";
                        }
                        // Collision (.hit)
                        const fs::path hit_file = sibling(".hit");
                        if (!hit_file.empty())
                        {
                            try
                            {
                                stage_hits[si] = sdbz::parse_hit(sdbz::read_file(hit_file));
                                stage_hits_loaded[si] = true;
                                has_stage_hit = true;
                                for (std::size_t oi = 0; oi < stage_hits[si].m_objects.size(); ++oi)
                                {
                                    stage_hit_by_names[si][stage_hits[si].m_objects[oi].m_name] = oi;
                                }
                                std::cout << "Stage collision " << hit_file.filename().string() << ": "
                                    << stage_hits[si].m_objects.size() << " objects\n";
                            }
                            catch (const std::exception& exception)
                            {
                                std::cerr << "Stage collision unavailable: " << exception.what() << '\n';
                            }
                        }
                        // Load stage textures (.pix) - deferred to after GL context creation
                        const fs::path pix_file = sibling(".pix");
                        if (!pix_file.empty())
                        {
                            stage_pix_paths[si] = pix_file;
                        }
                        // Compute world transforms
                        const auto& nodes = loaded_stages[si].m_nodes;
                        stage_worlds[si].resize(nodes.size());
                        stage_groups[si].assign(nodes.size(), 2);
                        for (std::size_t ni = 0; ni < nodes.size(); ++ni)
                        {
                            const auto& node = nodes[ni];
                            const mat4 local = trs_matrix(node.m_translation, node.m_rotation_radians, node.m_scale);
                            stage_worlds[si][ni] = (node.m_parent >= 0 && static_cast<std::size_t>(node.m_parent) < ni)
                                ? mat4_mul(stage_worlds[si][static_cast<std::size_t>(node.m_parent)], local)
                                : local;
                            std::size_t root = ni;
                            while (nodes[root].m_parent >= 0) root = static_cast<std::size_t>(nodes[root].m_parent);
                            stage_groups[si][ni] = root == loaded_stages[si].m_stage_root ? 0 : (root == loaded_stages[si].m_effect_root ? 1 : 2);
                        }
                    }
                }
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Stage load unavailable: " << exception.what() << '\n';
                return 2;
            }
    }

    // Auto-detect companion files for a model.
    if ((is_mdl || is_stg) && texture_path.empty())
    {
        texture_path = find_texture_for(input_path);
        if (!texture_path.empty()) std::cout << "Auto texture: " << texture_path.string() << '\n';
        else std::cout << "No matching .pix texture found next to the model\n";
    }
    if (is_mdl)
    {
        if (motion_paths.empty())
        {
            motion_paths = find_motions_for(input_path);
            for (const auto& p : motion_paths) std::cout << "Auto motion: " << p.string() << '\n';
            if (motion_paths.empty()) std::cout << "No .pmt motion package found next to the model\n";
        }
    }
    if (font_path.empty())
    {
        font_path = find_font_near(is_mdl || is_pix || is_hit || is_stg ? directory_of(input_path) : input_path);
        if (!font_path.empty()) std::cout << "Auto font: " << font_path.string() << '\n';
    }

    // Load every motion package and merge their clips into one list (N / B cycle through them).
    for (const auto& path : motion_paths)
    {
        try
        {
            auto package = sdbz::parse_rfsm(sdbz::read_file(path));
            std::cout << "Loaded motion package " << path.filename().string() << ": "
                << package.m_motions.size() << " motions, " << package.m_bone_names.size() << " bones\n";
            if (!has_loaded_motion)
            {
                loaded_motion = std::move(package);
                has_loaded_motion = true;
            }
            else
            {
                for (auto& clip : package.m_motions) loaded_motion.m_motions.push_back(std::move(clip));
            }
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Motion load unavailable (" << path.filename().string() << "): " << exception.what() << '\n';
            if (explicit_motion) return 2;
        }
    }
    if (has_loaded_motion && loaded_motion.m_motions.empty()) has_loaded_motion = false;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("Super Dragon Ball Z - Native Rebuild (OpenGL)",
        k_logical_width, k_logical_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
    {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    glEnable(GL_DEPTH_TEST);

    // Load textures AFTER OpenGL context is created
    if (!texture_path.empty() && std::filesystem::is_regular_file(texture_path))
    {
        if (has_extension(texture_path, ".pix"))
        {
            try
            {
                const auto file_data = sdbz::read_file(texture_path);
                texture_names = load_fpix_textures(file_data);
                has_textures = !texture_names.empty();
                std::cout << "Loaded " << texture_names.size() << " textures from "
                    << texture_path.string() << '\n';
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Texture load unavailable: " << exception.what() << '\n';
            }
        }
    }

    // Load FFON font AFTER OpenGL context is created
    if (!font_path.empty() && std::filesystem::is_regular_file(font_path))
    {
        if (has_extension(font_path, ".fon"))
        {
            try
            {
                has_font = load_ffon(font_path, font);
                if (has_font)
                {
                    glyph_textures.resize(font.glyph_count);
                    for (std::uint32_t gi = 0; gi < font.glyph_count; ++gi)
                    {
                        glyph_textures[gi] = upload_glyph_texture(font.glyphs[gi],
                            font.width, font.height, font.bytes_per_row);
                    }
                    std::cout << "Loaded " << glyph_textures.size() << " glyph textures\n";
                }
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Font load unavailable: " << exception.what() << '\n';
            }
        }
    }
    std::cout << "SDL init done\n";

    SDL_Gamepad* gamepad = SDL_OpenGamepad(0);

    // Load stage textures AFTER OpenGL context is created
    for (std::size_t si = 0; si < stage_pix_paths.size(); ++si)
    {
        if (!stage_pix_paths[si].empty())
        {
            try
            {
                const auto file_data = sdbz::read_file(stage_pix_paths[si]);
                stage_textures[si] = load_fpix_textures(file_data);
                stage_textures_loaded[si] = !stage_textures[si].empty();
                std::cout << "Stage textures " << stage_pix_paths[si].filename().string() << ": "
                    << stage_textures[si].size() << " textures\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Stage textures unavailable: " << exception.what() << '\n';
            }
        }
    }
    if (!gamepad)
    {
        std::cerr << "No gamepad available (keyboard only).\n";
    }

    s_input_state input;
    s_camera camera;
    bool camera_framed = false; // auto-fit once; F re-fits

    constexpr double k_simulation_rate = 60.0;
    constexpr double k_simulation_step = 1.0 / k_simulation_rate;
    double accumulator = 0.0;
    auto previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    bool running = true;
    bool left_button_down = false;
    bool pan_button_down = false; // right (or middle) mouse button
    int last_mouse_x = 0;
    int last_mouse_y = 0;
    while (running)
    {
        auto now = SDL_GetPerformanceCounter();
        double frame_time =
            (static_cast<double>(now - previous) / frequency);
        previous = now;
        if (frame_time > 0.25)
        {
            frame_time = 0.25;
        }
        accumulator += frame_time;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                }
                if (has_loaded_model && !part_groups.empty())
                {
                    // 1-9: cycle the variants of that part (then hidden); Shift cycles backwards; 0 resets.
                    const bool backwards = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                    if (event.key.key >= SDLK_1 && event.key.key <= SDLK_9)
                    {
                        const std::size_t gi = static_cast<std::size_t>(event.key.key - SDLK_1);
                        if (gi < part_groups.size())
                        {
                            auto& group = part_groups[gi];
                            const std::size_t states = group.m_meshes.size() + 1; // variants + hidden
                            group.m_active = backwards ? (group.m_active + states - 1) % states
                                                       : (group.m_active + 1) % states;
                            apply_part_visibility(part_groups, mesh_visible);
                            std::cout << group.m_label << ": "
                                << (group.m_active < group.m_meshes.size()
                                    ? loaded_model.m_meshes[group.m_meshes[group.m_active]].m_name
                                    : std::string("(hidden)"))
                                << '\n';
                        }
                    }
                    else if (event.key.key == SDLK_0)
                    {
                        for (auto& group : part_groups) group.m_active = 0;
                        apply_part_visibility(part_groups, mesh_visible);
                    }
                }
                if (has_loaded_stage)
                {
                    if (event.key.key == SDLK_E) show_effect = !show_effect;
                    else if (event.key.key == SDLK_C) show_collision = !show_collision;
                    else if (event.key.key == SDLK_T) show_templates = !show_templates;
                    else if (event.key.key == SDLK_PAGEUP)
                    {
                        loaded_stage_index = (loaded_stage_index + loaded_stages.size() - 1) % loaded_stages.size();
                        std::cout << "Stage: " << loaded_stages[loaded_stage_index].m_names[0] << '\n';
                    }
                    else if (event.key.key == SDLK_PAGEDOWN)
                    {
                        loaded_stage_index = (loaded_stage_index + 1) % loaded_stages.size();
                        std::cout << "Stage: " << loaded_stages[loaded_stage_index].m_names[0] << '\n';
                    }
                }
                // Camera zoom (multiplicative so it works at any model scale) and re-fit
                if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS)
                {
                    camera.m_distance = std::clamp(camera.m_distance * 0.85f, 0.05f, 1.0e6f);
                }
                else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS)
                {
                    camera.m_distance = std::clamp(camera.m_distance / 0.85f, 0.05f, 1.0e6f);
                }
                else if (event.key.key == SDLK_F)
                {
                    camera_framed = false;
                    camera.m_yaw = 0.0f;
                    camera.m_pitch = 0.0f;
                }
                if (has_loaded_motion)
                {
                    if (event.key.key == SDLK_N)
                    {
                        motion_clip_index = (motion_clip_index + 1) % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        std::cout << "Motion clip " << motion_clip_index << ": "
                            << loaded_motion.m_motions[motion_clip_index].m_name << '\n';
                    }
                    else if (event.key.key == SDLK_B)
                    {
                        motion_clip_index = (motion_clip_index + loaded_motion.m_motions.size() - 1)
                            % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        std::cout << "Motion clip " << motion_clip_index << ": "
                            << loaded_motion.m_motions[motion_clip_index].m_name << '\n';
                    }
                }
                update_key(input, event.key.key, true);
                break;
            case SDL_EVENT_KEY_UP:
                update_key(input, event.key.key, false);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_button_down = true;
                    last_mouse_x = event.button.x;
                    last_mouse_y = event.button.y;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE)
                {
                    pan_button_down = true;
                    last_mouse_x = event.button.x;
                    last_mouse_y = event.button.y;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_button_down = false;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE)
                {
                    pan_button_down = false;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (left_button_down)
                {
                    const int dx = event.motion.x - last_mouse_x;
                    const int dy = event.motion.y - last_mouse_y;
                    // Flip a sign to invert that axis of the mouse drag.
                    constexpr float k_drag_x = -0.5f;
                    constexpr float k_drag_y = -0.5f;
                    camera.m_yaw += dx * k_drag_x;
                    camera.m_pitch = std::clamp(camera.m_pitch + dy * k_drag_y, -85.0f, 85.0f);
                    last_mouse_x = event.motion.x;
                    last_mouse_y = event.motion.y;
                }
                else if (pan_button_down)
                {
                    // Pan: slide the camera target along the camera's right/up axes so the
                    // scene follows the cursor. Ortho view spans 2*distance vertically.
                    int window_w = 0, window_h = 0;
                    SDL_GetWindowSize(window, &window_w, &window_h);
                    const float units_per_pixel = 2.0f * camera.m_distance /
                        static_cast<float>(window_h > 0 ? window_h : 1);
                    const float dx = (event.motion.x - last_mouse_x) * units_per_pixel;
                    const float dy = (event.motion.y - last_mouse_y) * units_per_pixel;
                    constexpr float k_deg_to_rad = 0.01745329251994329577f;
                    const float a = -camera.m_yaw * k_deg_to_rad;
                    const float b = -camera.m_pitch * k_deg_to_rad;
                    const float sa = std::sin(a), ca = std::cos(a);
                    const float sb = std::sin(b), cb = std::cos(b);
                    const std::array<float, 3> right{ ca, 0.0f, sa };
                    const std::array<float, 3> up{ sa * sb, cb, -sb * ca };
                    for (int c = 0; c < 3; ++c)
                    {
                        camera.m_target[c] += -right[c] * dx + up[c] * dy;
                    }
                    last_mouse_x = static_cast<int>(event.motion.x);
                    last_mouse_y = static_cast<int>(event.motion.y);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                camera.m_distance = std::clamp(camera.m_distance * std::pow(0.88f, event.wheel.y), 0.05f, 1.0e6f);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (!gamepad)
                {
                    gamepad = SDL_OpenGamepad(event.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepad && SDL_GetGamepadID(gamepad) == event.gdevice.which)
                {
                    SDL_CloseGamepad(gamepad);
                    gamepad = nullptr;
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                update_gamepad_button(input, static_cast<SDL_GamepadButton>(
                    event.gbutton.button), true);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                update_gamepad_button(input, static_cast<SDL_GamepadButton>(
                    event.gbutton.button), false);
                break;
            default:
                break;
            }
        }

        while (accumulator >= k_simulation_step)
        {
            accumulator -= k_simulation_step;
        }

        int drawable_width = 0;
        int drawable_height = 0;
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        glViewport(0, 0, drawable_width, drawable_height);

        glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        apply_camera(camera, drawable_width, drawable_height);

        if (has_loaded_model && !loaded_model.m_meshes.empty())
        {
            // compute bind-pose world transforms (row-major-ish column array)
            const auto node_local = [](const sdbz::s_model_node& n) -> std::array<float, 16>
            {
                constexpr float k_deg_to_rad = 0.01745329251994329577f;
                const float x = n.m_rotation_degrees[0] * k_deg_to_rad * 0.5f;
                const float y = n.m_rotation_degrees[1] * k_deg_to_rad * 0.5f;
                const float z = n.m_rotation_degrees[2] * k_deg_to_rad * 0.5f;
                const float sx = std::sin(x), sy = std::sin(y), sz = std::sin(z);
                const float cx = std::cos(x), cy = std::cos(y), cz = std::cos(z);
                // Quaternion from Euler (matches arika-py quaternion function)
                const float qx = sx * cy * cz - cx * sy * sz;
                const float qy = cx * sy * cz + sx * cy * sz;
                const float qz = cx * cy * sz - sx * sy * cz;
                const float qw = cx * cy * cz + sx * sy * sz;
                // Quaternion to matrix (matches arika-py local_matrix)
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
            };
            const auto multiply4 = [](const std::array<float,16>& a,
                const std::array<float,16>& b) -> std::array<float,16>
            {
                std::array<float,16> out{};
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
            };
            const auto transform_point = [](const std::array<float,16>& m,
                const std::array<float,3>& p) -> std::array<float,3>
            {
                return std::array<float,3>{{
                    m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3],
                    m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7],
                    m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]
                }};
            };
            const auto build_world = [&](const std::vector<std::array<float,16>>& local)
                -> std::vector<std::array<float,16>>
            {
                std::vector<std::array<float,16>> w(local.size());
                std::vector<char> done(local.size(), 0);
                std::function<std::array<float,16>(std::size_t)> build =
                    [&](std::size_t index) -> std::array<float,16>
                {
                    if (done[index] == 2) return w[index];
                    if (done[index] == 1) throw std::runtime_error("hierarchy cycle");
                    done[index] = 1;
                    const auto local_mat = local[index];
                    const auto& node = loaded_model.m_nodes[index];
                    std::array<float,16> result;
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
            };
            // advance animation time
            if (has_loaded_motion && motion_clip_index < loaded_motion.m_motions.size())
            {
                animation_time += static_cast<float>(frame_time);
                const auto& clip = loaded_motion.m_motions[motion_clip_index];
                if (animation_time > clip.m_duration)
                {
                    animation_time = 0.0f;
                }
            }

            // local transform per node: animation sample overrides bind pose
            std::vector<std::array<float,16>> local_xform(loaded_model.m_nodes.size());
            for (std::size_t ni = 0; ni < loaded_model.m_nodes.size(); ++ni)
            {
                local_xform[ni] = node_local(loaded_model.m_nodes[ni]);
            }

            // bind pose world transforms (for drawing bones that match mesh vertices)
            std::vector<std::array<float,16>> bind_local(loaded_model.m_nodes.size());
            for (std::size_t ni = 0; ni < loaded_model.m_nodes.size(); ++ni)
            {
                bind_local[ni] = node_local(loaded_model.m_nodes[ni]);
            }
            std::vector<std::array<float,16>> bind_world = build_world(bind_local);
            const auto& bind_world_refs = bind_world;

            const auto mat4_inverse = [](const std::array<float,16>& m) -> std::array<float,16>
            {
                std::array<std::array<float,8>,4> aug{};
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
                    if (maxv < 1e-6f) return std::array<float,16>{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
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
                std::array<float,16> inv{};
                for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) inv[r * 4 + c] = aug[r][c + 4];
                return inv;
            };
            // inverse bind matrices for skinning: world[joint] * inverse_bind[joint] * pos
            std::vector<std::array<float,16>> inverse_bind(bind_world.size());
            for (std::size_t i = 0; i < bind_world.size(); ++i)
            {
                inverse_bind[i] = mat4_inverse(bind_world[i]);
            }

            if (has_loaded_motion && motion_clip_index < loaded_motion.m_motions.size())
            {
                const auto& clip = loaded_motion.m_motions[motion_clip_index];
                std::unordered_map<std::string, std::size_t> name_to_node;
                for (std::size_t ni = 0; ni < loaded_model.m_nodes.size(); ++ni)
                {
                    name_to_node[loaded_model.m_nodes[ni].m_name] = ni;
                }
                for (const auto& an : clip.m_nodes)
                {
                    auto it = name_to_node.find(an.m_name);
                    if (it == name_to_node.end())
                    {
                        continue;
                    }
                    const std::size_t mi = it->second;
                    std::array<float, 3> trans{ 0.0f, 0.0f, 0.0f };
                    std::array<float, 4> rot{ 0.0f, 0.0f, 0.0f, 1.0f };
                    std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
                    // Animated tracks are sampled; constant tracks hold one float4 in m_values.
                    if (!an.m_translation.m_values.empty())
                    {
                        const auto t = sdbz::sample_animation_track(an.m_translation, animation_time, false);
                        trans = { t[0], t[1], t[2] };
                    }
                    if (!an.m_rotation.m_values.empty())
                    {
                        const auto r = sdbz::sample_animation_track(an.m_rotation, animation_time, true);
                        rot = { r[0], r[1], r[2], r[3] };
                    }
                    if (!an.m_scale.m_values.empty())
                    {
                        const auto s = sdbz::sample_animation_track(an.m_scale, animation_time, false);
                        scale = { s[0], s[1], s[2] };
                    }
                    const float x = rot[0], y = rot[1], z = rot[2], w = rot[3];
                    std::array<float,16> rot_mat{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
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
                    std::array<float,16> trans_mat{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                    trans_mat[3] = trans[0]; trans_mat[7] = trans[1]; trans_mat[11] = trans[2];
                    std::array<float,16> scale_mat{ scale[0],0,0,0, 0,scale[1],0,0, 0,0,scale[2],0, 0,0,0,1 };
                    local_xform[mi] = multiply4(multiply4(trans_mat, rot_mat), scale_mat);
                }
            }

            std::vector<std::array<float,16>> world(loaded_model.m_nodes.size());
            std::vector<char> done(loaded_model.m_nodes.size(), 0);
            std::function<std::array<float,16>(std::size_t)> build =
                [&](std::size_t index) -> std::array<float,16>
            {
                if (done[index] == 2) return world[index];
                if (done[index] == 1) throw std::runtime_error("hierarchy cycle");
                done[index] = 1;
                const auto local = local_xform[index];
                const auto& node = loaded_model.m_nodes[index];
                std::array<float,16> result;
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
            for (std::size_t index = 0; index < loaded_model.m_nodes.size(); ++index)
            {
                build(index);
            }
            const auto& world_refs = world;

            auto skin_position = [&](const std::array<float, 3>& pos,
                const sdbz::s_model_vertex& v, std::int32_t rigid_node) -> std::array<float, 3>
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
                    if (joint >= loaded_model.m_nodes.size()) continue;
                    // bind pose -> joint-local (inverse_bind) -> animated world (world[joint])
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
                    // Non-skinned mesh: apply node world transform
                    // Rigid meshes (head, hair, hands...) are attached to the node stored on the
                    // mesh, not in the per-vertex joint array (which is always 0 for them).
                    std::size_t joint = rigid_node >= 0 ? static_cast<std::size_t>(rigid_node) : 0;
                    if (joint >= loaded_model.m_nodes.size()) joint = 0;
                    out = transform_point(world[joint], pos);
                }
                return out;
            };

            // Compute skinned bounding box for camera framing
            std::array<float, 3> skinned_min{ 1e9f, 1e9f, 1e9f };
            std::array<float, 3> skinned_max{ -1e9f, -1e9f, -1e9f };
            bool skinned_has_data = false;
            static const std::vector<sdbz::s_model_mesh> k_no_meshes;
            for (const auto& m : camera_framed ? k_no_meshes : loaded_model.m_meshes)
            {
                if (!mesh_visible.empty() &&
                    !mesh_visible[static_cast<std::size_t>(&m - &loaded_model.m_meshes[0])]) continue;
                if (!mesh_filters.empty())
                {
                    bool matches = false;
                    for (const auto& f : mesh_filters)
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
                            const auto sp = skin_position(v.m_position, v, m.m_rigid_node_index);
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
            if (!camera_framed && has_loaded_model && skinned_has_data)
            {
                camera_framed = true;
                camera.m_target = {
                    (skinned_min[0] + skinned_max[0]) * 0.5f,
                    (skinned_min[1] + skinned_max[1]) * 0.5f,
                    (skinned_min[2] + skinned_max[2]) * 0.5f,
                };
                const float extent = std::max({ skinned_max[0] - skinned_min[0],
                    skinned_max[1] - skinned_min[1],
                    skinned_max[2] - skinned_min[2] });
                camera.m_distance = std::max(extent * 0.75f, 0.1f);
                std::cout << "Skinned bbox: (" << skinned_min[0] << "," << skinned_min[1] << ","
                    << skinned_min[2] << ") to (" << skinned_max[0] << "," << skinned_max[1] << ","
                    << skinned_max[2] << ") extent=" << extent << " distance=" << camera.m_distance << '\n';
            }

            // draw skinned mesh with per-material textures and UVs
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
            for (const auto& mesh : loaded_model.m_meshes)
            {
                if (!mesh_filters.empty())
                {
                    bool matches = false;
                    for (const auto& filter : mesh_filters)
                    {
                        if (mesh.m_name.find(filter) != std::string::npos)
                        {
                            matches = true;
                            break;
                        }
                    }
                    if (!matches) continue;
                }
                if (!mesh_visible.empty() && !mesh_visible[static_cast<std::size_t>(&mesh - &loaded_model.m_meshes[0])]) continue;
                for (const auto& primitive : mesh.m_primitives)
                {
                    if (primitive.m_indices.size() < 3) continue;
                    const std::int32_t material_index = primitive.m_material_index;
                    bool                 has_texture = false;
                    GLuint tex = 0;
                    if (has_textures && material_index >= 0 &&
                        static_cast<std::size_t>(material_index) < loaded_model.m_materials.size())
                    {
                        const std::int32_t texture_index =
                            loaded_model.m_materials[material_index].m_texture_index;
                        if (texture_index >= 0 &&
                            static_cast<std::size_t>(texture_index) < texture_names.size())
                        {
                            tex = texture_names[texture_index];
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
                            const auto sp = skin_position(v.m_position, v, mesh.m_rigid_node_index);
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
        else if (has_loaded_hit)
        {
            glDisable(GL_LIGHTING);
            glColor3f(0.0f, 1.0f, 0.0f);
            glBegin(GL_LINES);
            for (const auto& poly : loaded_hit.m_polygons)
            {
                if (poly.m_vertex_count < 2) continue;
                for (std::uint32_t i = 0; i < poly.m_vertex_count; ++i)
                {
                    const std::uint32_t a = poly.m_first_vertex + i;
                    const std::uint32_t b = poly.m_first_vertex + ((i + 1) % poly.m_vertex_count);
                    if (a >= loaded_hit.m_vertices.size() || b >= loaded_hit.m_vertices.size()) continue;
                    const auto& va = loaded_hit.m_vertices[a].m_position;
                    const auto& vb = loaded_hit.m_vertices[b].m_position;
                    glVertex3fv(&va[0]);
                    glVertex3fv(&vb[0]);
                }
            }
            glEnd();
            glEnable(GL_LIGHTING);
        }
        // Stage scene: STG nodes place the sibling model's meshes; collision from the HIT.
        if (has_loaded_stage)
        {
            const auto group_visible = [&](std::size_t ni)
            {
                const char group = stage_groups[loaded_stage_index][ni];
                return group == 0 || (group == 1 && show_effect) || (group == 2 && show_templates);
            };

            if (has_stage_model && stage_mesh_lists[loaded_stage_index].empty())
            {
                stage_mesh_lists[loaded_stage_index].reserve(stage_models[loaded_stage_index].m_meshes.size());
                for (const auto& mesh : stage_models[loaded_stage_index].m_meshes)
                {
                    stage_mesh_lists[loaded_stage_index].push_back(compile_static_mesh_list(stage_models[loaded_stage_index], mesh, stage_textures[loaded_stage_index]));
                }
            }

            // Frame the camera on the visible placed geometry (once; F re-fits).
            if (!camera_framed)
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
                for (std::size_t ni = 0; ni < loaded_stages[loaded_stage_index].m_nodes.size(); ++ni)
                {
                    const auto& node = loaded_stages[loaded_stage_index].m_nodes[ni];
                    if (has_stage_model)
                    {
                        if (node.m_kind != 2) continue;
                    const auto it = stage_meshes_by_names[loaded_stage_index].find(node.m_name);
                    if (it == stage_meshes_by_names[loaded_stage_index].end()) continue;
                        for (const std::size_t mi : it->second)
                        {
                            for (const auto& v : stage_models[loaded_stage_index].m_meshes[mi].m_vertices)
                            {
                                extend(mat4_point(stage_worlds[loaded_stage_index][ni], v.m_position));
                            }
                        }
                    }
                    else
                    {
                        extend({ stage_worlds[loaded_stage_index][ni][3], stage_worlds[loaded_stage_index][ni][7], stage_worlds[loaded_stage_index][ni][11] });
                    }
                }
                if (any)
                {
                    camera_framed = true;
                    camera.m_target = { (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
                    const float extent = std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] });
                    camera.m_distance = std::max(extent * 0.6f, 0.1f);
                    camera.m_pitch = -30.0f; // look down on the stage
                    std::cout << "Stage bbox: (" << lo[0] << "," << lo[1] << "," << lo[2] << ") to ("
                        << hi[0] << "," << hi[1] << "," << hi[2] << ") distance=" << camera.m_distance << '\n';
                }
            }

            if (has_stage_model)
            {
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
                for (std::size_t ni = 0; ni < loaded_stages[loaded_stage_index].m_nodes.size(); ++ni)
                {
                    const auto& node = loaded_stages[loaded_stage_index].m_nodes[ni];
                    if (node.m_kind != 2 || !group_visible(ni)) continue;
                    const auto it = stage_meshes_by_names[loaded_stage_index].find(node.m_name);
                    if (it == stage_meshes_by_names[loaded_stage_index].end()) continue;
                    glPushMatrix();
                    gl_mult_matrix(stage_worlds[loaded_stage_index][ni]);
                    for (const std::size_t mi : it->second) glCallList(stage_mesh_lists[loaded_stage_index][mi]);
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
                for (std::size_t ni = 0; ni < loaded_stages[loaded_stage_index].m_nodes.size(); ++ni)
                {
                    const auto& node = loaded_stages[loaded_stage_index].m_nodes[ni];
                    if (node.m_parent < 0 || !group_visible(ni)) continue;
                    const auto& p = stage_worlds[loaded_stage_index][static_cast<std::size_t>(node.m_parent)];
                    const auto& c = stage_worlds[loaded_stage_index][ni];
                    glVertex3f(p[3], p[7], p[11]);
                    glVertex3f(c[3], c[7], c[11]);
                }
                glEnd();
            }

            if (show_collision && has_stage_hit)
            {
                glDisable(GL_LIGHTING);
                glDisable(GL_TEXTURE_2D);
                glColor3f(0.1f, 1.0f, 0.3f);
                for (std::size_t ni = 0; ni < loaded_stages[loaded_stage_index].m_nodes.size(); ++ni)
                {
                    const auto& node = loaded_stages[loaded_stage_index].m_nodes[ni];
                    if (node.m_kind != 3 || !group_visible(ni)) continue;
                    glPushMatrix();
                    gl_mult_matrix(stage_worlds[loaded_stage_index][ni]);
                    glBegin(GL_LINES);
                    for (const auto& reference : node.m_references)
                    {
                        const auto it = stage_hit_by_names[loaded_stage_index].find(reference.m_name);
                        if (it == stage_hit_by_names[loaded_stage_index].end()) continue;
                        const auto& object = stage_hits[loaded_stage_index].m_objects[it->second];
                        for (std::uint32_t pi = 0; pi < object.m_polygon_count; ++pi)
                        {
                            const std::size_t polygon_index = static_cast<std::size_t>(object.m_first_polygon) + pi;
                            if (polygon_index >= stage_hits[loaded_stage_index].m_polygons.size()) break;
                            const auto& polygon = stage_hits[loaded_stage_index].m_polygons[polygon_index];
                            for (std::uint32_t vi = 0; vi < polygon.m_vertex_count; ++vi)
                            {
                                const std::size_t a = static_cast<std::size_t>(polygon.m_first_vertex) + vi;
                                const std::size_t b = static_cast<std::size_t>(polygon.m_first_vertex) +
                                    ((vi + 1) % polygon.m_vertex_count);
                                if (a >= stage_hits[loaded_stage_index].m_vertices.size() || b >= stage_hits[loaded_stage_index].m_vertices.size()) continue;
                                const auto& pa = stage_hits[loaded_stage_index].m_vertices[a].m_position;
                                const auto& pb = stage_hits[loaded_stage_index].m_vertices[b].m_position;
                                glVertex3f(pa[0], pa[1], pa[2]);
                                glVertex3f(pb[0], pb[1], pb[2]);
                            }
                        }
                    }
                    glEnd();
                    glPopMatrix();
                }
            }
            glEnable(GL_LIGHTING);
        }
        else if (has_loaded_image && !texture_names.empty())
        {
            // Not used for single-image mode; textures are bound per-material above.
        }
        else if (has_loaded_image && !has_textures)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, upload_texture(loaded_image));

            const float scale = std::min(
                static_cast<float>(drawable_width) / static_cast<float>(loaded_image.m_width),
                static_cast<float>(drawable_height) / static_cast<float>(loaded_image.m_height));
            const float draw_width = static_cast<float>(loaded_image.m_width) * scale;
            const float draw_height = static_cast<float>(loaded_image.m_height) * scale;
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

        // On-screen HUD: motion name and mesh filters
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 1.0f, 1.0f);
        std::string hud = "INFO.DAT/GAME.DAT viewer";
        if (has_loaded_model) hud += " | model: " + loaded_model.m_name;
        if (has_loaded_stage) hud += " | stage: " + input_path.stem().string();
        if (has_loaded_motion) hud += " | motion: " + loaded_motion.m_motions[motion_clip_index].m_name;
        else if (is_mdl) hud += " | motion: none";
        if (!mesh_filters.empty())
        {
            hud += " | meshes:";
            for (const auto& f : mesh_filters) hud += " " + f;
        }
        if (has_font)
        {
            begin_2d(drawable_width, drawable_height);
            draw_text(hud, 10.0f, 10.0f, 1.0f, 255, 255, 0, font, glyph_textures);
            float line_y = 40.0f;
            for (std::size_t gi = 0; gi < part_groups.size() && gi < 9; ++gi)
            {
                const auto& group = part_groups[gi];
                const bool hidden = group.m_active >= group.m_meshes.size();
                std::string line = std::to_string(gi + 1) + " " + group.m_label + ": " +
                    (hidden ? std::string("hidden")
                            : loaded_model.m_meshes[group.m_meshes[group.m_active]].m_name) +
                    " [" + std::to_string(hidden ? 0 : group.m_active + 1) + "/" +
                    std::to_string(group.m_meshes.size()) + "]";
                if (hidden) draw_text(line, 10.0f, line_y, 0.8f, 140, 140, 140, font, glyph_textures);
                else        draw_text(line, 10.0f, line_y, 0.8f, 255, 255, 255, font, glyph_textures);
                line_y += 22.0f;
            }
            if (has_loaded_stage)
            {
                std::string stage_line = std::string("E effect: ") + (show_effect ? "on" : "off") +
                    " | C collision: " + (show_collision ? "on" : "off") +
                    " | T templates: " + (show_templates ? "on" : "off") + " | F fit camera";
                draw_text(stage_line, 10.0f, 40.0f, 0.8f, 255, 255, 255, font, glyph_textures);
            }
            if (!part_groups.empty())
            {
                draw_text("1-9 cycle parts | Shift reverse | 0 reset | F fit camera | right-drag pan", 10.0f, line_y + 6.0f,
                    0.7f, 160, 200, 255, font, glyph_textures);
            }
            // Camera info in bottom right
            {
                char cam_buf[128];
                std::snprintf(cam_buf, sizeof(cam_buf),
                    "Cam: yaw=%.1f pitch=%.1f dist=%.1f target=(%.1f,%.1f,%.1f)",
                    camera.m_yaw, camera.m_pitch, camera.m_distance,
                    camera.m_target[0], camera.m_target[1], camera.m_target[2]);
                std::string cam_str(cam_buf);
                // Estimate text width (6 px per char at scale 0.8)
                float text_width = static_cast<float>(cam_str.size()) * 6.0f * 0.8f;
                float cam_x = 10.0f;
                float cam_y = static_cast<float>(drawable_height) - 20.0f;
                draw_text(cam_str, cam_x, cam_y, 0.8f, 200, 200, 200, font, glyph_textures);
            }
            end_2d();
        }

        SDL_GL_SwapWindow(window);
        SDL_Delay(1);
    }

    for (const auto tex : texture_names)
    {
        if (tex) glDeleteTextures(1, &tex);
    }
    if (gamepad)
    {
        SDL_CloseGamepad(gamepad);
    }
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

 
 } 
 
 

