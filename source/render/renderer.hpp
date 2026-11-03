#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "assets/assets.hpp"
#include "core/animation.hpp"
#include "core/math.hpp"
#include "core/model.hpp"
#include "core/stage.hpp"
#include "core/texture.hpp"

namespace fs = std::filesystem;

// Camera state shared between platform input and the renderer.
struct s_camera
{
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_distance = 6.0f;
    std::array<float, 3> m_target{ 0.0f, 0.0f, 0.0f };
    bool m_framed = false; // auto-fit once; F re-fits
};

// One selectable debug-menu row (label + current value). The platform layer
// owns the action logic; the renderer only draws label/value.
struct s_menu_item
{
    std::string label;
    std::string value;
};

// All state required to render a viewer frame. Pointers are non-owning.
struct s_draw_context
{
    SDL_Window* window = nullptr;
    int drawable_width = 0;
    int drawable_height = 0;

    const s_camera* camera = nullptr;

    // Debug menu (F1 toggles). Filled by the platform layer each frame.
    bool debug_open = false;
    int debug_selected = 0;
    std::vector<s_menu_item> debug_menu;

    // Virtual file system browser (entered from the debug menu).
    bool vfs_open = false;
    fs::path vfs_path;
    int vfs_selected = 0;
    std::vector<std::pair<std::string, bool>> vfs_entries; // name, is_dir

    // Model viewer.
    const sdbz::s_model* model = nullptr;
    bool has_model = false;
    const std::vector<bool>* mesh_visible = nullptr;
    const std::vector<s_part_group>* part_groups = nullptr;
    std::vector<std::string> mesh_filters;

    // Motion viewer.
    const sdbz::s_motion_package* motion = nullptr;
    bool has_motion = false;
    std::size_t motion_clip_index = 0;
    float animation_time = 0.0f;

    // Stage viewer.
    const std::vector<sdbz::s_stage>* stages = nullptr;
    bool has_stage = false;
    std::size_t stage_index = 0;
    const std::vector<sdbz::s_model>* stage_models = nullptr;
    const std::vector<bool>* stage_models_loaded = nullptr;
    const std::vector<sdbz::s_hit>* stage_hits = nullptr;
    const std::vector<bool>* stage_hits_loaded = nullptr;
    const std::vector<std::vector<GLuint>>* stage_textures = nullptr;
    const std::vector<bool>* stage_textures_loaded = nullptr;
    const std::vector<std::vector<sdbz::mat4>>* stage_worlds = nullptr;
    const std::vector<std::vector<char>>* stage_groups = nullptr;
    const std::vector<std::unordered_map<std::string, std::vector<std::size_t>>>*
        stage_meshes_by_names = nullptr;
    const std::vector<std::unordered_map<std::string, std::size_t>>* stage_hit_by_names = nullptr;
    std::vector<std::vector<GLuint>>* stage_mesh_lists = nullptr;
    bool show_effect = false;
    bool show_collision = false;
    bool show_templates = false;
    bool has_stage_model = false;
    bool has_stage_hit = false;

    // Image viewer.
    const sdbz::s_texture_image* image = nullptr;
    bool has_image = false;
    bool has_textures = false;
    const std::vector<GLuint>* texture_names = nullptr;

    // Font / HUD text.
    const s_ffon_font* font = nullptr;
    bool has_font = false;
    const std::vector<GLuint>* glyph_textures = nullptr;

    fs::path input_path;
};

// Upload every glyph of an FFON font to OpenGL textures (after GL context exists).
void upload_glyph_textures(const s_ffon_font& font, std::vector<GLuint>& glyph_textures);

// Switch to a top-left pixel-space overlay (the 3D camera uses its own ortho/modelview).
void begin_2d(int width, int height);
void end_2d();

// Draw a single glyph as a textured quad (call between begin_2d/end_2d).
void draw_glyph(GLuint tex, float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b);

// Draw ASCII text using FFON glyphs (pixel coordinates, origin top-left).
void draw_text(const std::string& text, float x, float y, float scale,
    std::uint8_t r, std::uint8_t g, std::uint8_t b,
    const s_ffon_font& font, const std::vector<GLuint>& glyph_textures);

// Multiply the current OpenGL matrix by a row-major mat4.
void gl_mult_matrix(const sdbz::mat4& m);

// Compile one rigid mesh (all of its material primitives) into a display list.
GLuint compile_static_mesh_list(const sdbz::s_model& model,
    const sdbz::s_model_mesh& mesh,
    const std::vector<GLuint>& textures);

// Draw one complete viewer frame.
void render_frame(s_draw_context& ctx);
