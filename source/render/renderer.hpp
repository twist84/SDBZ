#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3/SDL_opengl.h>

#include "platform/window.hpp"

#include "assets/assets.hpp"
#include "core/animation.hpp"
#include "core/info_archive.hpp"
#include "core/math.hpp"
#include "core/model.hpp"
#include "core/stage.hpp"
#include "core/texture.hpp"
#include "gameplay/player_controller.hpp"
#include "gameplay/camera.hpp"

#include "interface/debug_menu/debug_menu.hpp"
#include "interface/part_groups.hpp"

namespace fs = std::filesystem;

// All state required to render a viewer frame. Pointers are non-owning.
struct s_draw_context
{
    c_window* window = nullptr;
    int drawable_width = 0;
    int drawable_height = 0;

    const sdbz::s_camera* camera = nullptr; // active render camera (viewer or gameplay)
    const sdbz::s_camera* viewer_camera = nullptr; // auto-framing always targets this camera
    const sdbz::s_player_controller* player_controller = nullptr;
    const sdbz::s_stage_collision_world* stage_collision_world = nullptr;
    bool player_controller_active = false;
    bool player_actor_placed = false;

    // Debug menu system (owned by platform layer).
    sdbz::s_debug_menu debug;

    // Virtual file system browser over an INFO.DAT/GAME.DAT archive.
    bool vfs_open = false;
    int vfs_selected = 0;
    const sdbz::s_archive* vfs_archive = nullptr;
    const std::vector<std::uint8_t>* vfs_game_data = nullptr;
    struct s_vfs_entry
    {
        std::size_t index = 0;      // index into archive entries
        std::string name;           // child name (directory or file)
        bool is_dir = false;
    };
    std::vector<s_vfs_entry> vfs_entries;
    std::vector<std::string> vfs_path_components; // breadcrumb path components

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
void draw_text(std::string_view text, float x, float y, float scale,
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
