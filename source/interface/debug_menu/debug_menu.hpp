#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL_opengl.h>

#include "assets/assets.hpp"
#include "core/animation.hpp"
#include "core/stage.hpp"
#include "platform/event.hpp"

namespace sdbz
{

// One selectable debug-menu row (label + current value).
struct s_menu_item
{
    std::string label;
    std::string value;
    bool is_submenu = false; // pressing enter opens a submenu instead of toggling
};

// Debug menu state. Home toggles, End goes back/closes, Enter activates.
// Max 26 items displayed per page; 0-9a-z quick-select by index.
struct s_debug_menu
{
    bool open = false;
    int selected = 0;
    std::vector<s_menu_item> items;    // current level items
    std::vector<int> stack;            // selected index per level (root = 0)
};

// Build the debug menu rows for the current frame.
// Returns the number of items built.
int debug_menu_build(s_debug_menu& menu,
    bool has_stage, const char* stage_name,
    bool has_motion, const char* motion_name,
    bool show_effect, bool show_collision, bool show_templates,
    bool archive_loaded);

// Handle debug menu input. Returns true if the event was consumed.
bool debug_menu_input(s_debug_menu& menu, const s_event& event,
    bool& running,
    bool& show_effect, bool& show_collision, bool& show_templates,
    bool& camera_framed, float& camera_yaw, float& camera_pitch,
    bool& vfs_open, std::vector<std::string>& vfs_path_components,
    int& vfs_selected,
    bool archive_loaded,
    bool has_stage, std::size_t& stage_index, const std::vector<s_stage>& stages,
    bool has_motion, std::size_t& motion_clip_index,
    const std::vector<s_animation_clip>& motions,
    float& animation_time);

// Draw the debug menu overlay.
void debug_menu_draw(const s_debug_menu& menu,
    int drawable_width, int drawable_height,
    const s_ffon_font& font, const std::vector<GLuint>& glyph_textures);

} // namespace sdbz
