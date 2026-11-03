#include "interface/debug_menu/debug_menu.hpp"

#include <algorithm>
#include <cstdio>
#include <print>

#include "render/renderer.hpp"

namespace sdbz
{

int debug_menu_build(s_debug_menu& menu,
    bool has_stage, const char* stage_name,
    bool has_motion, const char* motion_name,
    bool show_effect, bool show_collision, bool show_templates,
    bool archive_loaded)
{
    menu.items.clear();

    if (has_stage)
    {
        menu.items.push_back({ "Stage", stage_name, false });
    }
    if (has_motion)
    {
        menu.items.push_back({ "Motion", motion_name, false });
    }
    menu.items.push_back({ "Show effect", show_effect ? "on" : "off", false });
    menu.items.push_back({ "Show collision", show_collision ? "on" : "off", false });
    menu.items.push_back({ "Show templates", show_templates ? "on" : "off", false });
    menu.items.push_back({ "Archive VFS",
        archive_loaded ? "loaded" : "not loaded", false });
    menu.items.push_back({ "Fit camera", "", false });
    menu.items.push_back({ "Quit", "", false });

    if (menu.selected < 0 ||
        menu.selected >= static_cast<int>(menu.items.size()))
    {
        menu.selected = 0;
    }

    return static_cast<int>(menu.items.size());
}

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
    float& animation_time)
{
    if (!menu.open)
    {
        return false;
    }

    // Quick-select: 0-9 then a-z.
    const auto quick_select_index = [](e_key key) -> int
    {
        if (key >= e_key::digit_0 && key <= e_key::digit_9)
        {
            return static_cast<int>(key) - static_cast<int>(e_key::digit_0);
        }
        if (key >= e_key::a && key <= e_key::z)
        {
            return 10 + (static_cast<int>(key) - static_cast<int>(e_key::a));
        }
        return -1;
    };

    const int qs = quick_select_index(event.key);
    if (qs >= 0 && qs < static_cast<int>(menu.items.size()))
    {
        menu.selected = qs;
        return true;
    }

    // Navigation.
    if (event.key == e_key::up)
    {
        const int count = static_cast<int>(menu.items.size());
        menu.selected = (menu.selected + count - 1) % count;
        if (!menu.stack.empty())
        {
            menu.stack.back() = menu.selected;
        }
        return true;
    }
    if (event.key == e_key::down)
    {
        const int count = static_cast<int>(menu.items.size());
        menu.selected = (menu.selected + 1) % count;
        if (!menu.stack.empty())
        {
            menu.stack.back() = menu.selected;
        }
        return true;
    }

    // Enter activates.
    if (event.key == e_key::enter || event.key == e_key::space)
    {
        if (menu.items.empty())
        {
            return true;
        }
        const std::string& label = menu.items[menu.selected].label;

        // Archive VFS opens the archive browser directly (not a submenu).
        if (label == "Archive VFS")
        {
            if (archive_loaded)
            {
                vfs_open = true;
                menu.open = false;
                vfs_selected = 0;
                vfs_path_components.clear();
            }
            return true;
        }

        if (menu.items[menu.selected].is_submenu)
        {
            menu.stack.push_back(0);
            menu.selected = 0;
            return true;
        }

        if (label == "Stage" && has_stage && !stages.empty())
        {
            stage_index = (stage_index + 1) % stages.size();
            std::println("Stage: {}", stages[stage_index].m_names[0]);
            return true;
        }
        if (label == "Motion" && has_motion && !motions.empty())
        {
            motion_clip_index = (motion_clip_index + 1) % motions.size();
            animation_time = 0.0f;
            std::println("Motion clip {}: {}", motion_clip_index,
                motions[motion_clip_index].m_name);
            return true;
        }
        if (label == "Show effect")
        {
            show_effect = !show_effect;
            return true;
        }
        if (label == "Show collision")
        {
            show_collision = !show_collision;
            return true;
        }
        if (label == "Show templates")
        {
            show_templates = !show_templates;
            return true;
        }
        if (label == "Archive VFS")
        {
            if (archive_loaded)
            {
                vfs_open = true;
                menu.open = false;
                vfs_selected = 0;
                vfs_path_components.clear();
            }
            return true;
        }
        if (label == "Fit camera")
        {
            camera_framed = false;
            camera_yaw = 0.0f;
            camera_pitch = 0.0f;
            return true;
        }
        if (label == "Quit")
        {
            running = false;
            return true;
        }
    }

    return false;
}

void debug_menu_draw(const s_debug_menu& menu,
    int drawable_width, int drawable_height,
    const s_ffon_font& font, const std::vector<GLuint>& glyph_textures)
{
    if (!menu.open || menu.items.empty())
    {
        return;
    }

    begin_2d(drawable_width, drawable_height);

    const int depth = static_cast<int>(menu.stack.size());
    std::string title = "DEBUG MENU";
    for (int d = 0; d < depth; ++d)
    {
        title += " >";
    }
    draw_text(title, 10.0f, 10.0f, 1.0f, 255, 255, 0, font, glyph_textures);
    draw_text("Home: close  |  End: back  |  Enter: select  |  Arrows: navigate  |  0-9a-z: quick select",
        10.0f, 30.0f, 0.6f, 160, 160, 160, font, glyph_textures);

    // Max 26 items per page.
    constexpr int k_max_items = 26;
    int start = 0;
    if (menu.selected >= start + k_max_items)
    {
        start = menu.selected - k_max_items + 1;
    }
    if (menu.selected < start)
    {
        start = menu.selected;
    }
    if (start < 0)
    {
        start = 0;
    }
    if (start + k_max_items > static_cast<int>(menu.items.size()))
    {
        start = static_cast<int>(menu.items.size()) - k_max_items;
    }
    if (start < 0)
    {
        start = 0;
    }

    float y = 50.0f;
    const int end_idx = std::min(start + k_max_items,
        static_cast<int>(menu.items.size()));
    for (int i = start; i < end_idx; ++i)
    {
        const bool selected = (i == menu.selected);
        const std::uint8_t r = selected ? 255 : 200;
        const std::uint8_t g = selected ? 255 : 200;
        const std::uint8_t b = selected ? 100 : 200;

        // Quick-select key hint: 0-9 then a-z.
        std::string key_hint;
        if (i < 10)
        {
            key_hint = std::to_string(i);
        }
        else
        {
            key_hint = static_cast<char>('a' + i - 10);
        }

        std::string line = (selected ? "> " : "  ") + key_hint + ") " +
            menu.items[i].label;
        if (!menu.items[i].value.empty())
        {
            line += "  [" + menu.items[i].value + "]";
        }
        if (menu.items[i].is_submenu)
        {
            line += "  ▸";
        }
        draw_text(line, 10.0f, y, 0.8f, r, g, b, font, glyph_textures);
        y += 20.0f;
    }
    if (static_cast<int>(menu.items.size()) > k_max_items)
    {
        draw_text("...more items (scroll with arrows)", 10.0f, y, 0.6f,
            140, 140, 140, font, glyph_textures);
    }

    end_2d();
}

} // namespace sdbz
