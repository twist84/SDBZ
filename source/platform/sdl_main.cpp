#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "assets/assets.hpp"
#include "core/animation.hpp"
#include "core/math.hpp"
#include "core/model.hpp"
#include "core/stage.hpp"
#include "core/stage_collision.hpp"
#include "core/texture.hpp"
#include "gameplay/player_animation.hpp"
#include "gameplay/player_spawn.hpp"

#include "platform/event.hpp"
#include "platform/window.hpp"

#include "interface/part_groups.hpp"
#include "render/renderer.hpp"

namespace fs = std::filesystem;

namespace
{
float gamepad_axis_to_input(const Sint16 axis)
{
    constexpr float k_deadzone = 0.18f;
    const float value = std::clamp(static_cast<float>(axis) / 32767.0f, -1.0f, 1.0f);
    const float magnitude = std::abs(value);
    if (magnitude <= k_deadzone)
    {
        return 0.0f;
    }
    const float remapped = (magnitude - k_deadzone) / (1.0f - k_deadzone);
    return std::copysign(remapped, value);
}

void push_gamepad_menu_key(const SDL_Keycode key)
{
    SDL_Event key_event{};
    key_event.type = SDL_EVENT_KEY_DOWN;
    key_event.key.key = key;
    key_event.key.down = true;
    key_event.key.repeat = false;
    if (!SDL_PushEvent(&key_event))
    {
        std::println(std::cerr, "Failed to queue gamepad menu input: {}", SDL_GetError());
    }
}
} // namespace

int main(int argc, char** argv)
{
    fs::path input_path = "assets/extracted";
    std::vector<fs::path> motion_paths;
    fs::path texture_path;
    fs::path font_path;
    fs::path info_path;
    fs::path game_path;
    bool explicit_motion = false;
    std::vector<std::string> mesh_filters;
    std::span<char*> args(argv + 1, static_cast<size_t>(argc - 1));
    for (size_t i = 0; i < args.size(); ++i)
    {
        const std::string arg = args[i];
        if (arg == "--mesh" && i + 1 < args.size())
        {
            mesh_filters.push_back(args[++i]);
            continue;
        }
        if (arg == "--info" && i + 1 < args.size())
        {
            info_path = args[++i];
            continue;
        }
        if (arg == "--game" && i + 1 < args.size())
        {
            game_path = args[++i];
            continue;
        }
        else
        {
            input_path = arg;
            std::println("Input: {}", input_path.string());
        }
        const fs::path path = arg;
        if (has_extension(path, ".pmt") || has_extension(path, ".mot"))
        {
            motion_paths.push_back(path);
            explicit_motion = true;
        }
        else if (has_extension(path, ".pix"))
            texture_path = path;
        else if (has_extension(path, ".fon"))
            font_path = path;
        std::println(std::cerr, "Ignoring unrecognised argument: {}", arg);
    }
    if (input_path == "assets/extracted")
    {
        std::println("Input: {}", input_path.string());
    }
    // Archive state (INFO.DAT / GAME.DAT).
    sdbz::s_archive loaded_archive;
    bool has_loaded_archive = false;
    std::vector<std::uint8_t> game_data;
    bool has_game_data = false;
    // Auto-detect INFO.DAT/GAME.DAT next to the input path if not explicitly provided.
    if (!has_loaded_archive && !info_path.empty() && !game_path.empty() && fs::is_regular_file(info_path) &&
        fs::is_regular_file(game_path))
    {
        try
        {
            const auto info_data = sdbz::read_file(info_path);
            loaded_archive = sdbz::s_archive::from_encoded_info(info_data);
            has_loaded_archive = true;
            game_data = sdbz::read_file(game_path);
            has_game_data = true;
            std::println("Loaded archive: {} ({} entries) + {} ({} bytes)", info_path.filename().string(),
                         loaded_archive.m_entries.size(), game_path.filename().string(), game_data.size());
        }
        catch (const std::exception& exception)
        {
            std::println(std::cerr, "Archive load unavailable: {}", exception.what());
            has_loaded_archive = false;
            has_game_data = false;
        }
    }

    if (!has_loaded_archive)
    {
        const fs::path input_dir = fs::is_regular_file(input_path) ? input_path.parent_path() : input_path;
        const auto auto_info = input_dir / "INFO.DAT";
        const auto auto_game = input_dir / "GAME.DAT";
        if (fs::is_regular_file(auto_info) && fs::is_regular_file(auto_game))
        {
            try
            {
                const auto info_data = sdbz::read_file(auto_info);
                loaded_archive = sdbz::s_archive::from_encoded_info(info_data);
                has_loaded_archive = true;
                game_data = sdbz::read_file(auto_game);
                has_game_data = true;
                std::println("Auto-loaded archive: {} ({} entries) + {} ({} bytes)", auto_info.filename().string(),
                             loaded_archive.m_entries.size(), auto_game.filename().string(), game_data.size());
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Archive auto-load unavailable: " << exception.what() << '\n';
                has_loaded_archive = false;
                has_game_data = false;
            }
        }
    }

    if (!has_loaded_archive)
    {
        // Fallback to workingdir/assets/original/INFO.DAT + GAME.DAT.
        // Derive the working directory from the executable's location since
        // fs::current_path() may not match the cmd working directory.
        fs::path exe_dir = fs::absolute(argv[0]).parent_path();
        // Try walking up from the executable to find assets/original.
        std::vector<fs::path> candidate_dirs;
        candidate_dirs.push_back(fs::current_path());
        candidate_dirs.push_back(exe_dir);
        candidate_dirs.push_back(exe_dir.parent_path());
        candidate_dirs.push_back(exe_dir.parent_path().parent_path());
        candidate_dirs.push_back(exe_dir.parent_path().parent_path().parent_path());

        for (const auto& dir : candidate_dirs)
        {
            const auto fallback_info = dir / "assets" / "original" / "INFO.DAT";
            const auto fallback_game = dir / "assets" / "original" / "GAME.DAT";
            if (fs::is_regular_file(fallback_info) && fs::is_regular_file(fallback_game))
            {
                try
                {
                    const auto info_data = sdbz::read_file(fallback_info);
                    loaded_archive = sdbz::s_archive::from_encoded_info(info_data);
                    has_loaded_archive = true;
                    game_data = sdbz::read_file(fallback_game);
                    has_game_data = true;
                    std::println("Loaded archive fallback: {} ({} entries) + {} ({} bytes)", fallback_info.string(),
                                 loaded_archive.m_entries.size(), fallback_game.string(), game_data.size());
                    break;
                }
                catch (const std::exception& exception)
                {
                    std::println(std::cerr, "Archive auto-load fallback unavailable: {}", exception.what());
                    has_loaded_archive = false;
                    has_game_data = false;
                }
            }
        }
    }

    // Detect the input file type.
    bool is_pix = false;
    bool is_mdl = false;
    bool is_hit = false;
    bool is_stg = false;
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
    std::vector<bool> mesh_visible;
    std::vector<s_part_group> part_groups;

    // Stage scene (.stg places meshes from the sibling .mdl; .hit is collision).
    std::vector<sdbz::s_stage> loaded_stages;
    std::size_t loaded_stage_index = 0;
    bool has_loaded_stage = false;
    std::vector<sdbz::s_model> stage_models;
    std::vector<bool> stage_models_loaded;
    std::vector<sdbz::s_hit> stage_hits;
    std::vector<bool> stage_hits_loaded;
    std::vector<std::vector<GLuint>> stage_textures;
    std::vector<bool> stage_textures_loaded;
    std::vector<fs::path> stage_pix_paths;
    std::vector<std::size_t> stage_pix_entry_indices; // archive entry index per stage, or npos
    std::vector<std::vector<sdbz::mat4>> stage_worlds;
    std::vector<std::vector<char>> stage_groups;
    std::vector<std::unordered_map<std::string, std::vector<std::size_t>>> stage_meshes_by_names;
    std::vector<std::unordered_map<std::string, std::size_t>> stage_hit_by_names;
    std::vector<sdbz::s_stage_collision_world> stage_collision_worlds;
    std::vector<std::vector<GLuint>> stage_mesh_lists;
    bool show_effect = false;
    bool show_templates = false;
    bool show_collision = false;
    bool has_stage_model = false;
    bool has_stage_hit = false;

    // FFON font.
    s_ffon_font font;
    bool has_font = false;
    std::vector<std::uint8_t> archive_font_data;
    bool has_archive_font = false;
    std::vector<GLuint> glyph_textures;

    // ---- Asset discovery + parsing from disk (no GL context required) ----
    if (argc > 1 && fs::is_regular_file(input_path))
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
                std::print("Loaded texture: {}x{}", loaded_image.width, loaded_image.height);
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
                std::println("Loaded model: {} (meshes={}, materials={}, nodes={})", loaded_model.m_name,
                             loaded_model.m_meshes.size(), loaded_model.m_materials.size(),
                             loaded_model.m_nodes.size());
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
                            if (mesh_name.find(f) != std::string::npos)
                            {
                                matches = true;
                                break;
                            }
                        }
                        if (!matches)
                            continue;
                    }
                    const std::string label = part_base_name(mesh_name);
                    auto it = std::find_if(part_groups.begin(), part_groups.end(),
                                           [&](const s_part_group& g) { return g.m_label == label; });
                    if (it == part_groups.end())
                    {
                        part_groups.push_back({label, {}, 0});
                        it = part_groups.end() - 1;
                    }
                    it->m_meshes.push_back(mi);
                }
                apply_part_visibility(part_groups, mesh_visible);
                for (std::size_t gi = 0; gi < part_groups.size(); ++gi)
                {
                    std::println("{}: {} ({} meshes)", (gi < 9 ? std::to_string(gi + 1) : std::string("-")),
                                 part_groups[gi].m_label, part_groups[gi].m_meshes.size());
                    for (std::size_t vi = 0; vi < part_groups[gi].m_meshes.size(); ++vi)
                    {
                        if (vi == 0)
                        {
                            std::print("{}", loaded_model.m_meshes[part_groups[gi].m_meshes[vi]].m_name);
                            std::print(" (");
                        }
                        else
                        {
                            std::print(", {}", loaded_model.m_meshes[part_groups[gi].m_meshes[vi]].m_name);
                        }
                    }
                    std::println(")");
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
                std::println("Loaded hit: {} objects, {} polys, {} verts", loaded_hit.m_objects.size(),
                             loaded_hit.m_polygons.size(), loaded_hit.m_vertices.size());
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
                // Load ALL .stg files in the same folder (e.g. s03a.stg, s03b.stg).
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
                        std::println("Loaded stage: {} ({} nodes)", stg_path.filename().string(),
                                     loaded_stages.back().m_nodes.size());
                    }
                    catch (const std::exception& exception)
                    {
                        std::cerr << "Stage load failed for " << stg_path.filename().string() << ": "
                                  << exception.what() << '\n';
                    }
                }
                if (!loaded_stages.empty())
                {
                    has_loaded_stage = true;
                    loaded_stage_index = 0;
                    // Load companions for each stage.
                    for (std::size_t si = 0; si < loaded_stages.size(); ++si)
                    {
                        const auto& stg_path = stage_files[si];
                        const auto sibling = [&](std::string_view extension) -> fs::path
                        {
                            const std::string stem = lower_copy(stg_path.stem().string());
                            for (const auto& candidate : list_files(directory_of(stg_path), extension))
                            {
                                if (lower_copy(candidate.stem().string()) == stem)
                                    return candidate;
                            }
                            return {};
                        };
                        // Stage model (.mdl).
                        const fs::path model_file = sibling(".mdl");
                        if (!model_file.empty())
                        {
                            try
                            {
                                stage_models[si] =
                                    sdbz::parse_fmdl(sdbz::read_file(model_file), model_file.stem().string());
                                stage_models_loaded[si] = true;
                                has_stage_model = true;
                                std::println("Stage model {}: {} meshes", model_file.filename().string(),
                                             stage_models[si].m_meshes.size());
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
                            std::cerr << "No " << stg_path.stem().string()
                                      << ".mdl next to the stage; showing the node hierarchy only\n";
                        }
                        // Collision (.hit).
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
                                std::println("Stage collision {}: {} objects", hit_file.filename().string(),
                                             stage_hits[si].m_objects.size());
                            }
                            catch (const std::exception& exception)
                            {
                                std::cerr << "Stage collision unavailable: " << exception.what() << '\n';
                            }
                        }
                        // Load stage textures (.pix) - deferred to after GL context creation.
                        const fs::path pix_file = sibling(".pix");
                        if (!pix_file.empty())
                        {
                            stage_pix_paths[si] = pix_file;
                        }
                        // Compute world transforms.
                        const auto& nodes = loaded_stages[si].m_nodes;
                        stage_worlds[si].resize(nodes.size());
                        stage_groups[si].assign(nodes.size(), 2);
                        std::vector<sdbz::mat4> locals(nodes.size());
                        for (std::size_t ni = 0; ni < nodes.size(); ++ni)
                        {
                            const auto& node = nodes[ni];
                            locals[ni] = sdbz::trs_matrix(node.m_translation, node.m_rotation_radians, node.m_scale);
                        }
                        std::vector<std::uint8_t> xform_state(nodes.size(), 0);
                        for (std::size_t start = 0; start < nodes.size(); ++start)
                        {
                            if (xform_state[start] != 0)
                                continue;
                            std::vector<std::size_t> stack;
                            stack.push_back(start);
                            while (!stack.empty())
                            {
                                const std::size_t idx = stack.back();
                                if (xform_state[idx] == 2)
                                {
                                    stack.pop_back();
                                    continue;
                                }
                                if (xform_state[idx] == 1)
                                {
                                    const int parent = nodes[idx].m_parent;
                                    if (parent >= 0 && static_cast<std::size_t>(parent) < nodes.size())
                                    {
                                        stage_worlds[si][idx] = sdbz::mat4_mul(
                                            stage_worlds[si][static_cast<std::size_t>(parent)], locals[idx]);
                                    }
                                    else
                                    {
                                        stage_worlds[si][idx] = locals[idx];
                                    }
                                    xform_state[idx] = 2;
                                    stack.pop_back();
                                    continue;
                                }
                                if (xform_state[idx] == 0)
                                {
                                    const int parent = nodes[idx].m_parent;
                                    if (parent >= 0 && static_cast<std::size_t>(parent) < nodes.size())
                                    {
                                        if (xform_state[static_cast<std::size_t>(parent)] == 1)
                                        {
                                            stage_worlds[si][idx] = locals[idx];
                                            xform_state[idx] = 2;
                                            stack.pop_back();
                                            continue;
                                        }
                                        if (xform_state[static_cast<std::size_t>(parent)] == 0)
                                        {
                                            stack.push_back(static_cast<std::size_t>(parent));
                                        }
                                    }
                                    xform_state[idx] = 1;
                                    continue;
                                }
                            }
                        }
                        for (std::size_t ni = 0; ni < nodes.size(); ++ni)
                        {
                            std::size_t root = ni;
                            while (root < nodes.size() && nodes[root].m_parent >= 0)
                                root = static_cast<std::size_t>(nodes[root].m_parent);
                            stage_groups[si][ni] = (root == loaded_stages[si].m_stage_root)
                                                       ? 0
                                                       : ((root == loaded_stages[si].m_effect_root) ? 1 : 2);
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
    }

    // Auto-detect companion files for a model.
    if ((is_mdl || is_stg) && texture_path.empty())
    {
        texture_path = find_texture_for(input_path);
        std::println("Auto texture: {}", texture_path.string());
        std::println("No matching .pix texture found next to the model");
    }
    if (is_mdl)
    {
        if (motion_paths.empty())
        {
            motion_paths = find_motions_for(input_path);
            std::println("No .pmt motion package found next to the model");
        }
    }

    // Load every motion package and merge their clips into one list.
    for (const auto& path : motion_paths)
    {
        try
        {
            auto package = sdbz::parse_rfsm(sdbz::read_file(path));
            std::println("Loaded motion package {}: {} motions, {} bones", path.filename().string(),
                         package.m_motions.size(), package.m_bone_names.size());
            if (!has_loaded_motion)
            {
                loaded_motion = std::move(package);
                has_loaded_motion = true;
            }
            else
            {
                for (auto& clip : package.m_motions)
                    loaded_motion.m_motions.push_back(std::move(clip));
            }
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Motion load unavailable (" << path.filename().string() << "): " << exception.what() << '\n';
            if (explicit_motion)
                return 2;
        }
    }
    if (has_loaded_motion && loaded_motion.m_motions.empty())
        has_loaded_motion = false;

    // Prefer fonts from the archive when available.
    if (has_loaded_archive && has_game_data)
    {
        for (const auto& entry : loaded_archive.m_entries)
        {
            const std::string name = lower_copy(entry.m_name_cp932);
            if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".fon") == 0)
            {
                const std::uint64_t offset = entry.offset();
                const std::uint64_t byte_size = entry.m_byte_size;
                if (offset + byte_size <= game_data.size())
                {
                    archive_font_data.assign(game_data.data() + offset, game_data.data() + offset + byte_size);
                    has_archive_font = true;
                    std::println("Auto font from archive: {}", entry.m_name_cp932);
                    break;
                }
            }
        }
    }

    // If the archive provided no font, fall back to a .fon next to the input.
    if (!has_archive_font && font_path.empty())
    {
        font_path = find_font_near(is_mdl || is_pix || is_hit || is_stg ? directory_of(input_path) : input_path);
        std::println("Auto font: {}", font_path.string());
    }

    // ---- SDL / OpenGL / audio platform initialisation ----
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_AUDIO);

    c_window window;
    if (!window.create("Super Dragon Ball Z - Native Rebuild (OpenGL)", 1120, 490))
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    window.make_gl_current();
    glEnable(GL_DEPTH_TEST);

    // Open audio device.
    SDL_AudioSpec audio_spec{};
    audio_spec.freq = 44100;
    audio_spec.format = SDL_AUDIO_S16;
    audio_spec.channels = 1;
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec);
    if (!audio_device)
    {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << '\n';
    }
    else
    {
        std::println("Audio device opened: {} Hz, {} channels", audio_spec.freq, audio_spec.channels);
    }

    // Load textures AFTER OpenGL context is created.
    if (!texture_path.empty() && fs::is_regular_file(texture_path))
    {
        if (has_extension(texture_path, ".pix"))
        {
            try
            {
                texture_names = load_fpix_textures(texture_path);
                has_textures = !texture_names.empty();
                std::println("Loaded {} textures from {}", texture_names.size(), texture_path.string());
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Texture load unavailable: " << exception.what() << '\n';
            }
        }
    }

    // Load FFON font AFTER OpenGL context is created.
    if ((has_archive_font && !archive_font_data.empty()) || !font_path.empty())
    {
        try
        {
            if (has_archive_font && !archive_font_data.empty())
            {
                has_font = load_ffon(archive_font_data, font);
            }
            else
            {
                const auto font_data = sdbz::read_file(font_path);
                has_font = load_ffon(font_data, font);
            }
            if (has_font)
            {
                upload_glyph_textures(font, glyph_textures);
                std::println("Loaded {} glyph textures", glyph_textures.size());
            }
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Font load unavailable: " << exception.what() << '\n';
        }
    }
    std::println("SDL init done");

    SDL_Gamepad* gamepad = nullptr;
    bool gamepad_select_down = false;
    bool gamepad_start_down = false;
    bool gamepad_menu_chord_latched = false;
    int gamepad_count = 0;
    SDL_JoystickID* gamepad_ids = SDL_GetGamepads(&gamepad_count);
    if (gamepad_ids)
    {
        for (int gamepad_index = 0; gamepad_index < gamepad_count; ++gamepad_index)
        {
            gamepad = SDL_OpenGamepad(gamepad_ids[gamepad_index]);
            if (gamepad)
            {
                std::println("Gamepad connected: {}", SDL_GetGamepadName(gamepad));
                break;
            }
        }
        SDL_free(gamepad_ids);
    }

    // Load stage textures AFTER OpenGL context is created.
    for (std::size_t si = 0; si < stage_textures.size(); ++si)
    {
        if (!stage_pix_paths[si].empty())
        {
            try
            {
                const auto file_data = sdbz::read_file(stage_pix_paths[si]);
                stage_textures[si] = load_fpix_textures(file_data);
                stage_textures_loaded[si] = !stage_textures[si].empty();
                std::println("Loaded stage textures {}: {} textures", stage_pix_paths[si].filename().string(),
                             stage_textures[si].size());
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Stage texture load failed: " << exception.what() << '\n';
            }
        }
        else if (has_loaded_archive && stage_pix_entry_indices[si] != std::numeric_limits<std::size_t>::max() &&
                 stage_pix_entry_indices[si] < loaded_archive.m_entries.size())
        {
            try
            {
                const auto& entry = loaded_archive.m_entries[stage_pix_entry_indices[si]];
                const std::uint64_t offset = entry.offset();
                const std::uint64_t byte_size = entry.m_byte_size;
                if (offset + byte_size <= game_data.size())
                {
                    const std::span<const std::uint8_t> entry_data(game_data.data() + offset, byte_size);
                    stage_textures[si] =
                        load_fpix_textures(std::vector<std::uint8_t>(entry_data.begin(), entry_data.end()));
                    stage_textures_loaded[si] = !stage_textures[si].empty();
                    std::println("Loaded stage textures from archive: {}: {} textures", entry.m_name_cp932,
                                 stage_textures[si].size());
                }
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Archive stage texture load failed: " << exception.what() << '\n';
            }
        }
    }

    // ---- Viewer context, camera and provisional player controller ----
    sdbz::s_camera camera;
    sdbz::s_camera player_camera;
    player_camera.m_perspective = true;
    player_camera.m_orbit_distance = 8.0f;
    player_camera.m_vertical_fov_degrees = 55.0f;
    player_camera.m_pitch = 12.0f;
    player_camera.m_follow_offset = {0.0f, 1.35f, 0.0f};
    sdbz::s_player_controller player_controller;
    bool player_controller_active = false;
    bool player_actor_placed = false;
    bool player_spawn_valid = false;
    bool default_player_model_loaded = false;
    bool camera_follow_player = false;
    bool player_jump_requested = false;
    float player_land_animation_remaining = 0.0f;
    std::optional<sdbz::e_player_animation_state> active_player_animation_state;
    const auto player_spawn_overrides = sdbz::load_player_spawn_overrides("player_spawns.cfg");
    if (!player_spawn_overrides.empty())
    {
        std::println("Loaded {} player spawn override(s) from player_spawns.cfg", player_spawn_overrides.size());
    }
    sdbz::s_debug_menu debug_menu;
    bool vfs_open = false;
    int vfs_selected = 0;
    std::vector<std::string> vfs_path_components;

    // Size the player collision capsule from the currently loaded model's
    // axis-aligned bounding box (reference pose): radius from the XZ extent,
    // height from the Y extent.
    auto set_collision_capsule_from_model = [&]()
    {
        std::array<float, 3> bmin{ std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max(),
                                   std::numeric_limits<float>::max() };
        std::array<float, 3> bmax{ std::numeric_limits<float>::lowest(),
                                   std::numeric_limits<float>::lowest(),
                                   std::numeric_limits<float>::lowest() };
        bool has_data = false;
        for (const auto& mesh : loaded_model.m_meshes)
        {
            for (const auto& v : mesh.m_vertices)
            {
                bmin[0] = std::min(bmin[0], v.m_position[0]);
                bmin[1] = std::min(bmin[1], v.m_position[1]);
                bmin[2] = std::min(bmin[2], v.m_position[2]);
                bmax[0] = std::max(bmax[0], v.m_position[0]);
                bmax[1] = std::max(bmax[1], v.m_position[1]);
                bmax[2] = std::max(bmax[2], v.m_position[2]);
                has_data = true;
            }
        }
        if (!has_data) return;
        const float width_x = bmax[0] - bmin[0];
        const float width_z = bmax[2] - bmin[2];
        // Derive radius from the model's XZ extent but scale it down so the
        // capsule hugs the body rather than the hair/arm extremities.
        player_controller.m_collision_capsule_radius = std::max(width_x, width_z) * 0.25f;
        player_controller.m_collision_capsule_height = bmax[1] - bmin[1];
        // Jump apex = v^2 / (2g). Set jump speed so the apex equals the
        // collision capsule height.
        player_controller.m_jump_speed = std::sqrt(2.0f * player_controller.m_gravity *
            player_controller.m_collision_capsule_height);
        std::println("Collision capsule from model bounds: radius={} height={} jump_speed={} bounds=({}, {}, {})-({}, {}, {})",
            player_controller.m_collision_capsule_radius, player_controller.m_collision_capsule_height,
            player_controller.m_jump_speed,
            bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]);
    };

    // Load a model selected from the VFS, including its companion texture and
    // motion packages. Both manual VFS model selection and automatic player
    // loading use this path so their behaviour stays identical.
    auto load_archive_model = [&](const sdbz::s_archive_entry& model_entry)
    {
        const std::uint64_t model_offset = model_entry.offset();
        const std::uint64_t model_size = model_entry.m_byte_size;
        if (model_offset > game_data.size() || model_size > game_data.size() - model_offset)
        {
            throw std::out_of_range("model entry is outside GAME.DAT");
        }

        const std::span<const std::uint8_t> model_data(game_data.data() + model_offset,
                                                       static_cast<std::size_t>(model_size));
        const std::string model_name = fs::path(model_entry.m_name_cp932).stem().string();
        loaded_model = sdbz::parse_fmdl(model_data, model_name);
        has_loaded_model = true;
        set_collision_capsule_from_model();

        mesh_visible.assign(loaded_model.m_meshes.size(), false);
        part_groups.clear();
        for (std::size_t mi = 0; mi < loaded_model.m_meshes.size(); ++mi)
        {
            const auto& mesh_name = loaded_model.m_meshes[mi].m_name;
            const std::string label = part_base_name(mesh_name);
            auto it = std::find_if(part_groups.begin(), part_groups.end(),
                                   [&](const s_part_group& group) { return group.m_label == label; });
            if (it == part_groups.end())
            {
                part_groups.push_back({label, {}, 0});
                it = part_groups.end() - 1;
            }
            it->m_meshes.push_back(mi);
        }
        apply_part_visibility(part_groups, mesh_visible);
        std::println("Loaded model from archive: {}", model_entry.m_name_cp932);

        for (const GLuint texture : texture_names)
        {
            if (texture)
            {
                glDeleteTextures(1, &texture);
            }
        }
        texture_names.clear();
        has_textures = false;

        if (const auto* texture_entry = find_archive_texture_for(loaded_archive, model_entry.m_name_cp932))
        {
            const std::uint64_t texture_offset = texture_entry->offset();
            const std::uint64_t texture_size = texture_entry->m_byte_size;
            if (texture_offset <= game_data.size() && texture_size <= game_data.size() - texture_offset)
            {
                try
                {
                    const std::span<const std::uint8_t> texture_data(game_data.data() + texture_offset,
                                                                     static_cast<std::size_t>(texture_size));
                    texture_names =
                        load_fpix_textures(std::vector<std::uint8_t>(texture_data.begin(), texture_data.end()));
                    has_textures = !texture_names.empty();
                    std::println("Loaded model textures from archive: {} ({} textures)", texture_entry->m_name_cp932,
                                 texture_names.size());
                }
                catch (const std::exception& exception)
                {
                    std::println(std::cerr, "Archive texture load failed ({}): {}", texture_entry->m_name_cp932,
                                 exception.what());
                }
            }
            else
            {
                std::println(std::cerr, "Archive texture is outside GAME.DAT: {}", texture_entry->m_name_cp932);
            }
        }
        else
        {
            std::println(std::cerr, "No matching .pix texture found beside archive model: {}",
                         model_entry.m_name_cp932);
        }

        loaded_motion = {};
        has_loaded_motion = false;
        motion_clip_index = 0;
        animation_time = 0.0f;

        const auto motion_entries = find_archive_motions_for(loaded_archive, model_entry.m_name_cp932);
        for (const auto* motion_entry : motion_entries)
        {
            const std::uint64_t motion_offset = motion_entry->offset();
            const std::uint64_t motion_size = motion_entry->m_byte_size;
            if (motion_offset > game_data.size() || motion_size > game_data.size() - motion_offset)
            {
                std::println(std::cerr, "Archive motion is outside GAME.DAT: {}", motion_entry->m_name_cp932);
                continue;
            }

            try
            {
                const std::span<const std::uint8_t> motion_data(game_data.data() + motion_offset,
                                                                static_cast<std::size_t>(motion_size));
                auto package = sdbz::parse_rfsm(motion_data);
                const std::size_t package_motion_count = package.m_motions.size();
                if (!has_loaded_motion)
                {
                    loaded_motion = std::move(package);
                    has_loaded_motion = true;
                }
                else
                {
                    for (auto& clip : package.m_motions)
                    {
                        loaded_motion.m_motions.push_back(std::move(clip));
                    }
                }
                std::println("Loaded model motions from archive: {} ({} motions)", motion_entry->m_name_cp932,
                             package_motion_count);
            }
            catch (const std::exception& exception)
            {
                std::println(std::cerr, "Archive motion load failed ({}): {}", motion_entry->m_name_cp932,
                             exception.what());
            }
        }

        if (has_loaded_motion && loaded_motion.m_motions.empty())
        {
            has_loaded_motion = false;
        }
        if (!has_loaded_motion)
        {
            std::println("No matching .pmt motion packages found beside archive model: {}", model_entry.m_name_cp932);
        }
    };

    // The default player is stored inside GAME.DAT. Match the full VFS path,
    // not just the basename, so another p01a.mdl cannot be selected by mistake.
    auto find_archive_entry_by_path = [&](std::string_view archive_path) -> const sdbz::s_archive_entry*
    {
        std::string target(archive_path);
        std::replace(target.begin(), target.end(), '\\', '/');
        target = lower_copy(std::move(target));
        for (const auto& entry : loaded_archive.m_entries)
        {
            if (entry.m_is_directory)
            {
                continue;
            }
            std::string candidate = entry.m_name_cp932;
            std::replace(candidate.begin(), candidate.end(), '\\', '/');
            if (lower_copy(std::move(candidate)) == target)
            {
                return &entry;
            }
        }
        return nullptr;
    };

    auto load_default_player_from_archive = [&]()
    {
        constexpr std::string_view k_default_player_model = "ply/p01/p01a.mdl";
        default_player_model_loaded = false;
        if (!has_loaded_archive || !has_game_data)
        {
            std::println(std::cerr, "Cannot load default player {}: INFO.DAT/GAME.DAT is unavailable",
                         k_default_player_model);
            return false;
        }

        const auto* player_entry = find_archive_entry_by_path(k_default_player_model);
        if (!player_entry)
        {
            std::println(std::cerr, "Default player model not found in archive: {}", k_default_player_model);
            return false;
        }

        try
        {
            load_archive_model(*player_entry);
            default_player_model_loaded = true;
            std::println("Loaded default player model for stage: {}", player_entry->m_name_cp932);
            return true;
        }
        catch (const std::exception& exception)
        {
            std::println(std::cerr, "Default player model load failed ({}): {}", player_entry->m_name_cp932,
                         exception.what());
            return false;
        }
    };

    auto rebuild_stage_collision_world = [&](const std::size_t stage_index)
    {
        if (stage_index >= loaded_stages.size() || stage_index >= stage_hits.size() ||
            stage_index >= stage_hits_loaded.size() || stage_index >= stage_worlds.size())
        {
            return;
        }
        if (stage_collision_worlds.size() < loaded_stages.size())
        {
            stage_collision_worlds.resize(loaded_stages.size());
        }
        stage_collision_worlds[stage_index] = {};
        if (!stage_hits_loaded[stage_index])
        {
            std::println(std::cerr, "Stage {} has no HIT resource; player spawn cannot be resolved", stage_index);
            return;
        }
        stage_collision_worlds[stage_index] = sdbz::build_stage_collision_world(
            loaded_stages[stage_index], stage_hits[stage_index], stage_worlds[stage_index]);
        const auto& collision = stage_collision_worlds[stage_index];
        std::println("Stage collision world {}: {} polygons, {} unresolved references, {} degenerate polygons",
                     stage_index, collision.m_polygons.size(), collision.m_unresolved_references,
                     collision.m_degenerate_polygons);
    };

    // A stage transition starts a fresh player session. Keep this separate
    // from spawn resolution so a failed stage load cannot leave the previous
    // stage's actor, input state or follow camera active.
    auto reset_player_stage_session = [&]()
    {
        player_controller_active = false;
        camera_follow_player = false;
        player_spawn_valid = false;
        player_actor_placed = false;
        player_controller.m_position = {0.0f, 0.0f, 0.0f};
        player_controller.m_velocity = {0.0f, 0.0f, 0.0f};
        player_controller.m_fixed_time_accumulator = 0.0f;
        player_controller.m_jump_pressed_pending = false;
        player_controller.m_grounded = false;
        player_controller.m_ground_polygon_index = static_cast<std::size_t>(-1);
        player_controller.m_ground_object_name.clear();
        player_controller.m_ground_normal = {0.0f, 1.0f, 0.0f};
        player_controller.m_ground_height = 0.0f;
        player_controller.m_yaw_degrees = 0.0f;
        player_controller.m_running = false;
        player_controller.m_landed_this_frame = false;
        player_jump_requested = false;
        player_land_animation_remaining = 0.0f;
        active_player_animation_state.reset();
        animation_time = 0.0f;
        player_camera.m_target = {0.0f, 0.0f, 0.0f};
        player_camera.m_framed = false;
    };

    auto place_player_on_stage = [&]()
    {
        reset_player_stage_session();
        if (!has_loaded_stage || loaded_stage_index >= loaded_stages.size())
        {
            return false;
        }
        if (loaded_stage_index >= stage_collision_worlds.size())
        {
            rebuild_stage_collision_world(loaded_stage_index);
        }
        if (loaded_stage_index >= stage_collision_worlds.size())
        {
            std::println(std::cerr, "Player spawn unavailable: stage collision world was not built");
            player_controller_active = false;
            camera_follow_player = false;
            return false;
        }

        const auto& collision_world = stage_collision_worlds[loaded_stage_index];
        const auto& stage = loaded_stages[loaded_stage_index];
        const sdbz::s_player_spawn_override* selected_override = nullptr;
        if (!stage.m_names.empty())
        {
            // m_names.front() is also the stage identifier printed by the
            // viewer. Do not match overrides against every mesh/HIT object
            // name in the shared STG string table; those can collide with a
            // different stage's key.
            selected_override = sdbz::find_player_spawn_override(
                player_spawn_overrides, stage.m_names.front());
        }

        std::optional<sdbz::s_player_spawn> spawn;
        bool spawn_override_applied = false;
        if (selected_override)
        {
            spawn = sdbz::resolve_player_spawn_override(
                collision_world, *selected_override, player_controller.m_collision_foot_offset,
                player_controller.m_max_walkable_slope_degrees, player_controller.m_collision_capsule_radius,
                player_controller.m_collision_capsule_height);
            if (spawn)
            {
                spawn_override_applied = true;
            }
            else
            {
                std::println(
                    std::cerr,
                    "Player spawn override '{}' is not supported by this stage HIT; using collision-derived placement",
                    selected_override->m_stage_key);
            }
        }
        if (!spawn)
        {
            spawn = sdbz::find_stage_player_spawn(collision_world, player_controller.m_collision_foot_offset,
                                                  player_controller.m_max_walkable_slope_degrees,
                                                  player_controller.m_collision_capsule_radius,
                                                  player_controller.m_collision_capsule_height);
        }
        if (!spawn)
        {
            std::println(
                std::cerr,
                "Player spawn unavailable for stage {}: no walkable HIT polygon was found; refusing Y=0 fallback",
                loaded_stages[loaded_stage_index].m_names.empty() ? std::string("<unnamed>")
                                                                  : loaded_stages[loaded_stage_index].m_names[0]);
            player_controller_active = false;
            camera_follow_player = false;
            return false;
        }

        player_controller.m_position = spawn->m_position;
        player_controller.m_velocity = {0.0f, 0.0f, 0.0f};
        player_controller.m_ground_normal = spawn->m_normal;
        player_controller.m_ground_polygon_index = spawn->m_collision_polygon_index;
        player_controller.m_ground_object_name = spawn->m_object_name;
        player_controller.m_ground_height = spawn->m_position[1] - player_controller.m_collision_foot_offset;
        player_controller.m_yaw_degrees =
            spawn_override_applied && selected_override ? selected_override->m_yaw_degrees : 0.0f;
        player_controller.m_grounded = true;
        player_spawn_valid = true;
        player_actor_placed = has_loaded_model && default_player_model_loaded;
        player_camera.m_target = {player_controller.m_position[0] + player_camera.m_follow_offset[0],
                                  player_controller.m_position[1] + player_camera.m_follow_offset[1],
                                  player_controller.m_position[2] + player_camera.m_follow_offset[2]};
        player_camera.m_framed = true;
        std::println("Player spawn: HIT object='{}' node={} polygon={} position=({}, {}, {}) area={}",
                     spawn->m_object_name, spawn->m_stage_node_index, spawn->m_collision_polygon_index,
                     spawn->m_position[0], spawn->m_position[1], spawn->m_position[2], spawn->m_surface_area);
        std::println(
            "  stage key(s) for player_spawns.cfg: {} | {} from origin (XZ) | source: {}",
            stage.m_names.empty() ? std::string("<unnamed>") : stage.m_names[0],
            std::sqrt(spawn->m_position[0] * spawn->m_position[0] + spawn->m_position[2] * spawn->m_position[2]),
            spawn_override_applied ? "player_spawns.cfg override" : "collision heuristic");
        return true;
    };

    // Build transformed collision surfaces for disk-loaded stages now that all
    // STG world matrices and companion HIT resources are available.
    if (stage_collision_worlds.size() < loaded_stages.size())
    {
        stage_collision_worlds.resize(loaded_stages.size());
    }
    for (std::size_t stage_index = 0; stage_index < loaded_stages.size(); ++stage_index)
    {
        rebuild_stage_collision_world(stage_index);
    }

    // A stage supplied on the command line is already loaded at this point.
    // Populate its player actor and place it on an authored HIT support surface.
    if (has_loaded_stage)
    {
        load_default_player_from_archive();
        place_player_on_stage();
    }

    s_draw_context ctx;
    ctx.window = &window;
    ctx.camera = &camera;
    ctx.viewer_camera = &camera;
    ctx.player_controller = &player_controller;
    ctx.stage_collision_world = has_loaded_stage && loaded_stage_index < stage_collision_worlds.size()
                                    ? &stage_collision_worlds[loaded_stage_index]
                                    : nullptr;
    ctx.player_controller_active = player_controller_active;
    ctx.player_actor_placed = player_actor_placed;
    ctx.model = has_loaded_model ? &loaded_model : nullptr;
    ctx.has_model = has_loaded_model;
    ctx.mesh_visible = &mesh_visible;
    ctx.part_groups = &part_groups;
    ctx.mesh_filters = mesh_filters;
    ctx.motion = has_loaded_motion ? &loaded_motion : nullptr;
    ctx.has_motion = has_loaded_motion;
    ctx.motion_clip_index = motion_clip_index;
    ctx.animation_time = animation_time;
    ctx.stages = has_loaded_stage ? &loaded_stages : nullptr;
    ctx.has_stage = has_loaded_stage;
    ctx.stage_index = loaded_stage_index;
    ctx.stage_models = &stage_models;
    ctx.stage_models_loaded = &stage_models_loaded;
    ctx.stage_hits = &stage_hits;
    ctx.stage_hits_loaded = &stage_hits_loaded;
    ctx.stage_textures = &stage_textures;
    ctx.stage_textures_loaded = &stage_textures_loaded;
    ctx.stage_worlds = &stage_worlds;
    ctx.stage_groups = &stage_groups;
    ctx.stage_meshes_by_names = &stage_meshes_by_names;
    ctx.stage_hit_by_names = &stage_hit_by_names;
    ctx.stage_mesh_lists = &stage_mesh_lists;
    ctx.show_effect = show_effect;
    ctx.show_collision = show_collision;
    ctx.show_templates = show_templates;
    ctx.has_stage_model = has_stage_model;
    ctx.has_stage_hit = has_stage_hit;
    ctx.image = has_loaded_image ? &loaded_image : nullptr;
    ctx.has_image = has_loaded_image;
    ctx.has_textures = has_textures;
    ctx.texture_names = &texture_names;
    ctx.font = has_font ? &font : nullptr;
    ctx.has_font = has_font;
    ctx.glyph_textures = &glyph_textures;
    ctx.input_path = input_path;
    ctx.debug = debug_menu;

    // ---- Main loop ----
    bool running = true;
    bool left_button_down = false;
    bool pan_button_down = false; // right (or middle) mouse button
    int last_mouse_x = 0;
    int last_mouse_y = 0;
    int cur_mouse_x = 0;
    int cur_mouse_y = 0;

    auto previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running)
    {
        auto now = SDL_GetPerformanceCounter();
        double frame_time = (static_cast<double>(now - previous) / frequency);
        previous = now;
        if (frame_time > 0.25)
        {
            frame_time = 0.25;
        }

        player_jump_requested = false;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                // Home toggles the debug menu regardless of state.
                if (event.key.key == SDLK_HOME)
                {
                    if (vfs_open)
                    {
                        vfs_open = false;
                        vfs_selected = 0;
                        vfs_path_components.clear();
                    }
                    else
                    {
                        debug_menu.open = !debug_menu.open;
                        debug_menu.selected = 0;
                    }
                    break;
                }
                // End goes back a menu level or closes on root.
                if (event.key.key == SDLK_END)
                {
                    if (vfs_open)
                    {
                        if (!vfs_path_components.empty())
                        {
                            vfs_path_components.pop_back();
                            vfs_selected = 0;
                        }
                        else
                        {
                            vfs_open = false;
                            vfs_selected = 0;
                        }
                        break;
                    }
                    if (debug_menu.stack.size() > 1)
                    {
                        debug_menu.stack.pop_back();
                        debug_menu.selected = debug_menu.stack.back();
                    }
                    else
                    {
                        debug_menu.open = false;
                        debug_menu.selected = 0;
                    }
                    break;
                }
                if (debug_menu.open && !vfs_open)
                {
                    s_event se{};
                    se.type = e_event_type::key_down;
                    se.key = sdl_key_to_e_key(static_cast<int>(event.key.key));
                    se.mod = static_cast<int>(event.key.mod);
                    if (debug_menu_input(debug_menu, se, running, show_effect, show_collision, show_templates,
                                         camera.m_framed, camera.m_yaw, camera.m_pitch, vfs_open, vfs_path_components,
                                         vfs_selected, has_loaded_archive, has_loaded_stage, loaded_stage_index,
                                         loaded_stages, has_loaded_motion, motion_clip_index, loaded_motion.m_motions,
                                         animation_time))
                    {
                        break;
                    }
                }
                if (vfs_open)
                {
                    // Virtual file system navigation over archive entries.
                    // Extra scope prevents 'count' initialization from being
                    // skipped by subsequent switch case labels.
                    {
                        const int count = static_cast<int>(ctx.vfs_entries.size());
                        if (count == 0)
                            break;
                        if (event.key.key == SDLK_UP)
                        {
                            vfs_selected = (vfs_selected + count - 1) % count;
                        }
                        else if (event.key.key == SDLK_DOWN)
                        {
                            vfs_selected = (vfs_selected + 1) % count;
                        }
                        else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE ||
                                 event.key.key == SDLK_RIGHT)
                        {
                            if (count == 0)
                                break;
                            const auto& ve = ctx.vfs_entries[vfs_selected];
                            const std::size_t entry_index = ve.index;
                            const auto& entry = loaded_archive.m_entries[entry_index];
                            const std::string& name = entry.m_name_cp932;
                            if (ve.is_dir)
                            {
                                // Drill down into directory.
                                vfs_path_components.push_back(ve.name);
                                vfs_selected = 0;
                            }
                            else
                            {
                                // Directories need only archive metadata. File contents
                                // require GAME.DAT to have been loaded successfully.
                                if (!has_game_data)
                                    break;

                                // Load file based on extension.
                                const std::uint64_t offset = entry.offset();
                                const std::uint64_t byte_size = entry.m_byte_size;
                                if (offset + byte_size > game_data.size())
                                    break;
                                const std::span<const std::uint8_t> entry_data(game_data.data() + offset, byte_size);
                                const std::string ext = fs::path(name).extension().string();
                                try
                                {
                                    if (ext == ".mdl" || ext == ".MDL")
                                    {
                                        load_archive_model(entry);
                                    }
                                    else if (ext == ".pix" || ext == ".PIX")
                                    {
                                        for (GLuint t : texture_names)
                                            if (t)
                                                glDeleteTextures(1, &t);
                                        texture_names = load_fpix_textures(
                                            std::vector<std::uint8_t>(entry_data.begin(), entry_data.end()));
                                        has_textures = !texture_names.empty();
                                        std::println("Loaded textures from archive: {}", texture_names.size());
                                    }
                                    else if (ext == ".fon" || ext == ".FON")
                                    {
                                        for (GLuint t : glyph_textures)
                                            if (t)
                                                glDeleteTextures(1, &t);
                                        glyph_textures.clear();
                                        has_font = load_ffon(
                                            std::vector<std::uint8_t>(entry_data.begin(), entry_data.end()), font);
                                        if (has_font)
                                        {
                                            upload_glyph_textures(font, glyph_textures);
                                            std::println("Loaded font from archive: {} glyphs", glyph_textures.size());
                                        }
                                    }
                                    else if (ext == ".pmt" || ext == ".mot")
                                    {
                                        auto package = sdbz::parse_rfsm(entry_data);
                                        loaded_motion = std::move(package);
                                        has_loaded_motion = true;
                                        motion_clip_index = 0;
                                        animation_time = 0.0f;
                                        player_land_animation_remaining = 0.0f;
                                        active_player_animation_state.reset();
                                        std::println("Loaded motions from archive: {}", loaded_motion.m_motions.size());
                                    }
                                    else if (ext == ".stg" || ext == ".STG")
                                    {
                                        // Invalidate the old player session before touching stage
                                        // resources. If parsing or companion loading fails, the old
                                        // stage's actor and camera must not remain active.
                                        reset_player_stage_session();
                                        default_player_model_loaded = false;
                                        try
                                        {
                                            // Clear any previously loaded stage to avoid memory buildup.
                                            for (auto& lists : stage_mesh_lists)
                                                for (GLuint l : lists)
                                                    if (l)
                                                        glDeleteLists(l, 1);
                                            for (auto& texs : stage_textures)
                                                for (GLuint t : texs)
                                                    if (t)
                                                        glDeleteTextures(1, &t);
                                            stage_mesh_lists.clear();
                                            stage_mesh_lists.shrink_to_fit();
                                            loaded_stages.clear();
                                            loaded_stages.shrink_to_fit();
                                            stage_models.clear();
                                            stage_models.shrink_to_fit();
                                            stage_models_loaded.clear();
                                            stage_models_loaded.shrink_to_fit();
                                            stage_hits.clear();
                                            stage_hits.shrink_to_fit();
                                            stage_hits_loaded.clear();
                                            stage_hits_loaded.shrink_to_fit();
                                            stage_textures.clear();
                                            stage_textures.shrink_to_fit();
                                            stage_textures_loaded.clear();
                                            stage_textures_loaded.shrink_to_fit();
                                            stage_pix_paths.clear();
                                            stage_pix_paths.shrink_to_fit();
                                            stage_pix_entry_indices.clear();
                                            stage_pix_entry_indices.shrink_to_fit();
                                            stage_worlds.clear();
                                            stage_worlds.shrink_to_fit();
                                            stage_groups.clear();
                                            stage_groups.shrink_to_fit();
                                            stage_meshes_by_names.clear();
                                            stage_meshes_by_names.shrink_to_fit();
                                            stage_hit_by_names.clear();
                                            stage_hit_by_names.shrink_to_fit();
                                            stage_collision_worlds.clear();
                                            stage_collision_worlds.shrink_to_fit();
                                            has_loaded_stage = false;
                                            has_stage_model = false;
                                            has_stage_hit = false;
                                            loaded_stage_index = 0;

                                            const std::string stage_stem = fs::path(name).stem().string();
                                            sdbz::s_stage stg = sdbz::parse_stg(entry_data);

                                            // Find companion files in the archive by stem name.
                                            // Archive entries may carry path prefixes (e.g.
                                            // "stg/s00/s00a.mdl") so match by stem suffix.
                                            auto find_archive_entry =
                                                [&](std::string_view extension) -> const sdbz::s_archive_entry*
                                            {
                                                const std::string target =
                                                    lower_copy(stage_stem + std::string(extension));
                                                for (const auto& candidate : loaded_archive.m_entries)
                                                {
                                                    const std::string name = lower_copy(candidate.m_name_cp932);
                                                    if (name == target || (name.size() >= target.size() &&
                                                                           name.compare(name.size() - target.size(),
                                                                                        target.size(), target) == 0))
                                                    {
                                                        return &candidate;
                                                    }
                                                }
                                                return nullptr;
                                            };

                                            // Stage model (.mdl).
                                            stage_meshes_by_names.emplace_back();
                                            const auto* model_entry = find_archive_entry(".mdl");
                                            if (model_entry)
                                            {
                                                const std::uint64_t moffset = model_entry->offset();
                                                const std::uint64_t msize = model_entry->m_byte_size;
                                                if (moffset + msize <= game_data.size())
                                                {
                                                    const std::span<const std::uint8_t> mdata(
                                                        game_data.data() + moffset, msize);
                                                    stage_models.emplace_back(
                                                        sdbz::parse_fmdl(mdata, model_entry->m_name_cp932));
                                                    stage_models_loaded.push_back(true);
                                                    has_stage_model = true;
                                                    std::size_t mi = stage_models.size() - 1;
                                                    for (std::size_t mxi = 0; mxi < stage_models[mi].m_meshes.size();
                                                         ++mxi)
                                                    {
                                                        stage_meshes_by_names
                                                            .back()[stage_models[mi].m_meshes[mxi].m_name]
                                                            .push_back(mxi);
                                                    }
                                                    std::println("Stage model from archive: {} ({} meshes)",
                                                                 model_entry->m_name_cp932,
                                                                 stage_models.back().m_meshes.size());
                                                }
                                            }
                                            else
                                            {
                                                std::cerr << "No " << stage_stem
                                                          << ".mdl in archive; showing node hierarchy only\n";
                                                stage_models.emplace_back();
                                                stage_models_loaded.push_back(false);
                                            }

                                            // Collision (.hit). Keep the stage vectors aligned even
                                            // when a companion is missing or malformed.
                                            stage_hit_by_names.emplace_back();
                                            stage_hits.emplace_back();
                                            stage_hits_loaded.push_back(false);
                                            const auto* hit_entry = find_archive_entry(".hit");
                                            if (hit_entry)
                                            {
                                                const std::uint64_t hoffset = hit_entry->offset();
                                                const std::uint64_t hsize = hit_entry->m_byte_size;
                                                if (hoffset <= game_data.size() && hsize <= game_data.size() - hoffset)
                                                {
                                                    try
                                                    {
                                                        const std::span<const std::uint8_t> hdata(
                                                            game_data.data() + hoffset,
                                                            static_cast<std::size_t>(hsize));
                                                        stage_hits.back() = sdbz::parse_hit(hdata);
                                                        stage_hits_loaded.back() = true;
                                                        has_stage_hit = true;
                                                        for (std::size_t oi = 0;
                                                             oi < stage_hits.back().m_objects.size(); ++oi)
                                                        {
                                                            stage_hit_by_names
                                                                .back()[stage_hits.back().m_objects[oi].m_name] = oi;
                                                        }
                                                        std::println("Stage collision from archive: {} ({} objects)",
                                                                     hit_entry->m_name_cp932,
                                                                     stage_hits.back().m_objects.size());
                                                    }
                                                    catch (const std::exception& exception)
                                                    {
                                                        std::println(std::cerr, "Stage HIT parse failed ({}): {}",
                                                                     hit_entry->m_name_cp932, exception.what());
                                                    }
                                                }
                                            }

                                            // Stage textures (.pix) - loaded lazily after GL context.
                                            const auto* pix_entry = find_archive_entry(".pix");
                                            stage_pix_paths.push_back(fs::path());
                                            stage_textures.emplace_back();
                                            stage_textures_loaded.push_back(false);
                                            if (pix_entry)
                                            {
                                                const std::size_t pix_index = static_cast<std::size_t>(
                                                    pix_entry - loaded_archive.m_entries.data());
                                                stage_pix_entry_indices.push_back(pix_index);
                                                try
                                                {
                                                    const auto& pix_entry_ref = loaded_archive.m_entries[pix_index];
                                                    const std::uint64_t pix_offset = pix_entry_ref.offset();
                                                    const std::uint64_t pix_byte_size = pix_entry_ref.m_byte_size;
                                                    if (pix_offset + pix_byte_size <= game_data.size())
                                                    {
                                                        const std::span<const std::uint8_t> pix_data(
                                                            game_data.data() + pix_offset, pix_byte_size);
                                                        stage_textures.back() =
                                                            load_fpix_textures(std::vector<std::uint8_t>(
                                                                pix_data.begin(), pix_data.end()));
                                                        stage_textures_loaded.back() = !stage_textures.back().empty();
                                                        std::println(
                                                            "Loaded stage textures from archive: {}: {} textures",
                                                            pix_entry_ref.m_name_cp932, stage_textures.back().size());
                                                    }
                                                }
                                                catch (const std::exception& exception)
                                                {
                                                    std::cerr
                                                        << "Archive stage texture load failed: " << exception.what()
                                                        << '\n';
                                                }
                                            }
                                            else
                                            {
                                                stage_pix_entry_indices.push_back(
                                                    std::numeric_limits<std::size_t>::max());
                                            }

                                            loaded_stages.push_back(std::move(stg));
                                            stage_worlds.emplace_back();
                                            stage_groups.emplace_back();
                                            stage_mesh_lists.emplace_back();

                                            // Compute world transforms for the new stage.
                                            const std::size_t si = loaded_stages.size() - 1;
                                            const auto& nodes = loaded_stages[si].m_nodes;
                                            stage_worlds[si].resize(nodes.size());
                                            stage_groups[si].assign(nodes.size(), 2);
                                            std::vector<sdbz::mat4> locals(nodes.size());
                                            for (std::size_t ni = 0; ni < nodes.size(); ++ni)
                                            {
                                                const auto& node = nodes[ni];
                                                locals[ni] = sdbz::trs_matrix(node.m_translation,
                                                                              node.m_rotation_radians, node.m_scale);
                                            }
                                            std::vector<std::uint8_t> xform_state(nodes.size(), 0);
                                            for (std::size_t start = 0; start < nodes.size(); ++start)
                                            {
                                                if (xform_state[start] != 0)
                                                    continue;
                                                std::vector<std::size_t> stack;
                                                stack.push_back(start);
                                                while (!stack.empty())
                                                {
                                                    const std::size_t idx = stack.back();
                                                    if (xform_state[idx] == 2)
                                                    {
                                                        stack.pop_back();
                                                        continue;
                                                    }
                                                    if (xform_state[idx] == 1)
                                                    {
                                                        const int parent = nodes[idx].m_parent;
                                                        if (parent >= 0 &&
                                                            static_cast<std::size_t>(parent) < nodes.size())
                                                        {
                                                            stage_worlds[si][idx] = sdbz::mat4_mul(
                                                                stage_worlds[si][static_cast<std::size_t>(parent)],
                                                                locals[idx]);
                                                        }
                                                        else
                                                        {
                                                            stage_worlds[si][idx] = locals[idx];
                                                        }
                                                        xform_state[idx] = 2;
                                                        stack.pop_back();
                                                        continue;
                                                    }
                                                    if (xform_state[idx] == 0)
                                                    {
                                                        const int parent = nodes[idx].m_parent;
                                                        if (parent >= 0 &&
                                                            static_cast<std::size_t>(parent) < nodes.size())
                                                        {
                                                            if (xform_state[static_cast<std::size_t>(parent)] == 1)
                                                            {
                                                                stage_worlds[si][idx] = locals[idx];
                                                                xform_state[idx] = 2;
                                                                stack.pop_back();
                                                                continue;
                                                            }
                                                            if (xform_state[static_cast<std::size_t>(parent)] == 0)
                                                            {
                                                                stack.push_back(static_cast<std::size_t>(parent));
                                                            }
                                                        }
                                                        xform_state[idx] = 1;
                                                        continue;
                                                    }
                                                }
                                            }
                                            for (std::size_t ni = 0; ni < nodes.size(); ++ni)
                                            {
                                                std::size_t root = ni;
                                                while (root < nodes.size() && nodes[root].m_parent >= 0)
                                                    root = static_cast<std::size_t>(nodes[root].m_parent);
                                                stage_groups[si][ni] =
                                                    (root == loaded_stages[si].m_stage_root)
                                                        ? 0
                                                        : ((root == loaded_stages[si].m_effect_root) ? 1 : 2);
                                            }

                                            has_loaded_stage = true;
                                            loaded_stage_index = loaded_stages.size() - 1;
                                            camera.m_framed = false; // reframe the independent world camera
                                            rebuild_stage_collision_world(loaded_stage_index);
                                            load_default_player_from_archive();
                                            place_player_on_stage();
                                            std::println("Loaded stage from archive: {} ({} nodes)", name,
                                                         loaded_stages.back().m_nodes.size());
                                        }
                                        catch (const std::exception& exception)
                                        {
                                            std::cerr << "Stage load from archive failed (" << name
                                                      << "): " << exception.what() << '\n';
                                        }
                                    }
                                    else if (ext == ".hit" || ext == ".HIT")
                                    {
                                        loaded_hit = sdbz::parse_hit(entry_data);
                                        has_loaded_hit = true;
                                        std::println("Loaded hit from archive: {} objects",
                                                     loaded_hit.m_objects.size());
                                    }
                                    else
                                    {
                                        std::println("Archive entry not loadable: {}", name);
                                    }
                                }
                                catch (const std::exception& exception)
                                {
                                    std::cerr << "Archive entry load failed (" << name << "): " << exception.what()
                                              << '\n';
                                }
                            }
                            break;
                        }
                        else if (event.key.key == SDLK_BACKSPACE || event.key.key == SDLK_LEFT)
                        {
                            // Go up one directory level.
                            if (!vfs_path_components.empty())
                            {
                                vfs_path_components.pop_back();
                                vfs_selected = 0;
                            }
                            break;
                        }
                        else if (event.key.key == SDLK_ESCAPE)
                        {
                            vfs_open = false;
                            vfs_selected = 0;
                            break;
                        }
                    }
                }
                if (!debug_menu.open && !vfs_open && !event.key.repeat)
                {
                    if (event.key.key == SDLK_P)
                    {
                        if (!has_loaded_model)
                        {
                            std::println("Load a character model before enabling the player controller.");
                        }
                        else if (has_loaded_stage && !player_spawn_valid)
                        {
                            std::println(std::cerr,
                                         "Player control unavailable: this stage has no validated HIT spawn surface.");
                        }
                        else if (has_loaded_stage && !default_player_model_loaded)
                        {
                            std::println(std::cerr,
                                         "Player control unavailable: the default player model did not load.");
                        }
                        else
                        {
                            player_controller_active = !player_controller_active;
                            if (player_controller_active)
                            {
                                if (!has_loaded_stage)
                                {
                                    // Model-only preview retains the old flat-ground fallback.
                                    player_controller.m_position = {
                                        camera.m_target[0], player_controller.m_ground_height, camera.m_target[2]};
                                    player_controller.m_velocity = {0.0f, 0.0f, 0.0f};
                                    player_controller.m_fixed_time_accumulator = 0.0f;
                                    player_controller.m_jump_pressed_pending = false;
                                    player_controller.m_grounded = true;
                                    player_actor_placed = true;
                                }
                                player_camera.m_follow_offset = {0.0f, 1.35f, 0.0f};
                                player_camera.m_target = {
                                    player_controller.m_position[0] + player_camera.m_follow_offset[0],
                                    player_controller.m_position[1] + player_camera.m_follow_offset[1],
                                    player_controller.m_position[2] + player_camera.m_follow_offset[2]};
                                player_camera.m_framed = true;
                                camera_follow_player = true;
                                std::println(
                                    "Player controller enabled: WASD / left stick move, Shift / LB run, Space / A "
                                    "jump. Mouse orbits gameplay camera; wheel changes signed orbit distance.");
                            }
                            else
                            {
                                camera_follow_player = false;
                                std::println("Player controller disabled; world viewer camera restored.");
                            }
                        }
                        break;
                    }
                    else if (event.key.key == SDLK_C && player_controller_active)
                    {
                        player_camera.m_follow_offset = {0.0f, 1.35f, 0.0f};
                        player_camera.m_target = {player_controller.m_position[0] + player_camera.m_follow_offset[0],
                                                  player_controller.m_position[1] + player_camera.m_follow_offset[1],
                                                  player_controller.m_position[2] + player_camera.m_follow_offset[2]};
                        camera_follow_player = true;
                        player_camera.m_framed = true;
                        break;
                    }
                    else if (event.key.key == SDLK_SPACE && player_controller_active)
                    {
                        player_jump_requested = true;
                    }
                    else if (event.key.key == SDLK_R && player_controller_active)
                    {
                        // Unstuck: teleport back to a valid stage spawn so the
                        // player can recover from being embedded in collision.
                        place_player_on_stage();
                        std::println("Player unstuck: respawned at stage spawn");
                    }
                }
                if (event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                }
                // Model viewer part cycling.
                if (has_loaded_model && !part_groups.empty())
                {
                    const bool backwards = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                    if (event.key.key >= SDLK_1 && event.key.key <= SDLK_9)
                    {
                        const std::size_t gi = static_cast<std::size_t>(event.key.key - SDLK_1);
                        if (gi < part_groups.size())
                        {
                            auto& group = part_groups[gi];
                            const std::size_t states = group.m_meshes.size() + 1; // variants + hidden
                            group.m_active =
                                backwards ? (group.m_active + states - 1) % states : (group.m_active + 1) % states;
                            apply_part_visibility(part_groups, mesh_visible);
                            std::println("{}: {}", group.m_label,
                                         group.m_active < group.m_meshes.size()
                                             ? loaded_model.m_meshes[group.m_meshes[group.m_active]].m_name
                                             : std::string("(hidden)"));
                        }
                    }
                    else if (event.key.key == SDLK_0)
                    {
                        for (auto& group : part_groups)
                            group.m_active = 0;
                        apply_part_visibility(part_groups, mesh_visible);
                    }
                }
                // Stage viewer toggles.
                if (has_loaded_stage)
                {
                    if (event.key.key == SDLK_E)
                        show_effect = !show_effect;
                    else if (event.key.key == SDLK_T)
                        show_templates = !show_templates;
                    else if (event.key.key == SDLK_H)
                        show_collision = !show_collision;
                    else if (event.key.key == SDLK_PAGEUP)
                    {
                        loaded_stage_index = (loaded_stage_index + loaded_stages.size() - 1) % loaded_stages.size();
                        std::println("Stage: {}", loaded_stages[loaded_stage_index].m_names[0]);
                        has_stage_model =
                            loaded_stage_index < stage_models_loaded.size() && stage_models_loaded[loaded_stage_index];
                        has_stage_hit =
                            loaded_stage_index < stage_hits_loaded.size() && stage_hits_loaded[loaded_stage_index];
                        camera.m_framed = false;
                        place_player_on_stage();
                    }
                    else if (event.key.key == SDLK_PAGEDOWN)
                    {
                        loaded_stage_index = (loaded_stage_index + 1) % loaded_stages.size();
                        std::println("Stage: {}", loaded_stages[loaded_stage_index].m_names[0]);
                        has_stage_model =
                            loaded_stage_index < stage_models_loaded.size() && stage_models_loaded[loaded_stage_index];
                        has_stage_hit =
                            loaded_stage_index < stage_hits_loaded.size() && stage_hits_loaded[loaded_stage_index];
                        camera.m_framed = false;
                        place_player_on_stage();
                    }
                }
                // Camera zoom and re-fit.
                if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS)
                {
                    if (player_controller_active)
                    {
                        sdbz::zoom_orbit_camera(player_camera, 1.0f);
                    }
                    else
                    {
                        camera.m_distance = std::clamp(camera.m_distance * 0.85f, 0.05f, 1.0e6f);
                    }
                }
                else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS)
                {
                    if (player_controller_active)
                    {
                        sdbz::zoom_orbit_camera(player_camera, -1.0f);
                    }
                    else
                    {
                        camera.m_distance = std::clamp(camera.m_distance / 0.85f, 0.05f, 1.0e6f);
                    }
                }
                else if (event.key.key == SDLK_F)
                {
                    camera.m_framed = false;
                    camera.m_yaw = 0.0f;
                    camera.m_pitch = 0.0f;
                }
                // Motion clip cycling.
                if (has_loaded_motion && !player_controller_active)
                {
                    if (event.key.key == SDLK_N)
                    {
                        motion_clip_index = (motion_clip_index + 1) % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        active_player_animation_state.reset();
                        std::println("Motion clip {}: {}", motion_clip_index,
                                     loaded_motion.m_motions[motion_clip_index].m_name);
                    }
                    else if (event.key.key == SDLK_B)
                    {
                        motion_clip_index =
                            (motion_clip_index + loaded_motion.m_motions.size() - 1) % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        active_player_animation_state.reset();
                        std::println("Motion clip {}: {}", motion_clip_index,
                                     loaded_motion.m_motions[motion_clip_index].m_name);
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_button_down = true;
                    last_mouse_x = static_cast<int>(event.button.x);
                    last_mouse_y = static_cast<int>(event.button.y);
                    cur_mouse_x = last_mouse_x;
                    cur_mouse_y = last_mouse_y;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE)
                {
                    pan_button_down = true;
                    last_mouse_x = static_cast<int>(event.button.x);
                    last_mouse_y = static_cast<int>(event.button.y);
                    cur_mouse_x = last_mouse_x;
                    cur_mouse_y = last_mouse_y;
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
                cur_mouse_x = static_cast<int>(event.motion.x);
                cur_mouse_y = static_cast<int>(event.motion.y);
                if (left_button_down)
                {
                    const int dx = static_cast<int>(event.motion.x) - last_mouse_x;
                    const int dy = static_cast<int>(event.motion.y) - last_mouse_y;
                    // Flip a sign to invert that axis of the mouse drag.
                    constexpr float k_drag_x = -0.5f;
                    constexpr float k_drag_y = -0.5f;
                    if (player_controller_active)
                    {
                        sdbz::orbit_camera(player_camera, dx * k_drag_x, dy * k_drag_y);
                    }
                    else
                    {
                        camera.m_yaw += dx * k_drag_x;
                        camera.m_pitch = std::clamp(camera.m_pitch + dy * k_drag_y, -85.0f, 85.0f);
                    }
                    last_mouse_x = static_cast<int>(event.motion.x);
                    last_mouse_y = static_cast<int>(event.motion.y);
                }
                else if (pan_button_down)
                {
                    // Pan: slide the camera target along the camera's right/up axes so the
                    // scene follows the cursor. Ortho view spans 2*distance vertically.
                    int window_w = 0, window_h = 0;
                    window.get_size_pixels(window_w, window_h);
                    const float units_per_pixel =
                        player_controller_active
                            ? std::max(std::abs(player_camera.m_orbit_distance), 0.15f) * 2.0f /
                                  static_cast<float>(window_h > 0 ? window_h : 1)
                            : 2.0f * camera.m_distance / static_cast<float>(window_h > 0 ? window_h : 1);
                    const float dx = (event.motion.x - last_mouse_x) * units_per_pixel;
                    const float dy = (event.motion.y - last_mouse_y) * units_per_pixel;
                    constexpr float k_deg_to_rad = 0.01745329251994329577f;
                    const float active_yaw =
                        player_controller_active
                            ? player_camera.m_yaw + (player_camera.m_orbit_distance < 0.0f ? 180.0f : 0.0f)
                            : camera.m_yaw;
                    const float active_pitch =
                        player_controller_active
                            ? player_camera.m_pitch * (player_camera.m_orbit_distance < 0.0f ? -1.0f : 1.0f)
                            : camera.m_pitch;
                    const float a = -active_yaw * k_deg_to_rad;
                    const float b = -active_pitch * k_deg_to_rad;
                    const float sa = std::sin(a), ca = std::cos(a);
                    const float sb = std::sin(b), cb = std::cos(b);
                    const std::array<float, 3> right{ca, 0.0f, sa};
                    const std::array<float, 3> up{-sa * sb, cb, sb * ca};
                    for (int c = 0; c < 3; ++c)
                    {
                        const float pan_delta = -right[c] * dx + up[c] * dy;
                        if (player_controller_active)
                        {
                            player_camera.m_follow_offset[static_cast<std::size_t>(c)] += pan_delta;
                        }
                        else
                        {
                            camera.m_target[static_cast<std::size_t>(c)] += pan_delta;
                        }
                    }
                    last_mouse_x = static_cast<int>(event.motion.x);
                    last_mouse_y = static_cast<int>(event.motion.y);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
            {
                // Zoom toward the mouse cursor: keep the world point under the
                // cursor at the same screen position as the ortho volume scales.
                const float wheel = static_cast<float>(event.wheel.y);
                if (player_controller_active)
                {
                    sdbz::zoom_orbit_camera(player_camera, wheel);
                    break;
                }
                const float prev_distance = camera.m_distance;
                camera.m_distance = std::clamp(camera.m_distance * std::pow(0.88f, wheel), 0.05f, 1.0e6f);
                const float dd = prev_distance - camera.m_distance;
                if (dd != 0.0f && !camera_follow_player)
                {
                    int window_w = 0, window_h = 0;
                    window.get_size_pixels(window_w, window_h);
                    if (window_w > 0 && window_h > 0)
                    {
                        // View-space delta between old and new projection at the cursor.
                        // dv = (dd/h) * (2*mx - w, h - 2*my, 0)
                        const float scale = dd / static_cast<float>(window_h);
                        const float dv_x =
                            scale * (2.0f * static_cast<float>(cur_mouse_x) - static_cast<float>(window_w));
                        const float dv_y =
                            scale * (static_cast<float>(window_h) - 2.0f * static_cast<float>(cur_mouse_y));
                        // Rotate view-space delta into world space by the inverse
                        // camera rotation (yaw then pitch).
                        const float k_deg_to_rad = 0.01745329251994329577f;
                        const float yaw = camera.m_yaw * k_deg_to_rad;
                        const float pitch = camera.m_pitch * k_deg_to_rad;
                        const float cy = std::cos(yaw), sy = std::sin(yaw);
                        const float cp = std::cos(pitch), sp = std::sin(pitch);
                        camera.m_target[0] += dv_x * cy + dv_y * sp * sy;
                        camera.m_target[1] += dv_y * cp;
                        camera.m_target[2] += -dv_x * sy + dv_y * (sp * cy - cp * sy);
                    }
                }
                camera.m_framed = true;
                break;
            }
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
                    gamepad_select_down = false;
                    gamepad_start_down = false;
                    gamepad_menu_chord_latched = false;
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (!gamepad || event.gbutton.which != SDL_GetGamepadID(gamepad))
                {
                    break;
                }

                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)
                {
                    gamepad_select_down = true;
                }
                else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_START)
                {
                    gamepad_start_down = true;
                }

                // Select + Start toggles the debug menu. Require both buttons
                // to be released before another toggle can occur, otherwise
                // holding one and tapping the other would open/close repeatedly.
                if (gamepad_select_down && gamepad_start_down && !gamepad_menu_chord_latched)
                {
                    if (vfs_open)
                    {
                        vfs_open = false;
                        vfs_selected = 0;
                        vfs_path_components.clear();
                    }
                    else
                    {
                        debug_menu.open = !debug_menu.open;
                        debug_menu.selected = 0;
                        debug_menu.stack.clear();
                    }
                    gamepad_menu_chord_latched = true;
                    break;
                }

                if (gamepad_menu_chord_latched)
                {
                    break;
                }

                if (debug_menu.open || vfs_open)
                {
                    SDL_Keycode menu_key = SDLK_UNKNOWN;
                    switch (event.gbutton.button)
                    {
                    case SDL_GAMEPAD_BUTTON_DPAD_UP:
                        menu_key = SDLK_UP;
                        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                        menu_key = SDLK_DOWN;
                        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                        menu_key = SDLK_LEFT;
                        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                        menu_key = SDLK_RIGHT;
                        break;
                    case SDL_GAMEPAD_BUTTON_SOUTH: // A / Cross
                        menu_key = SDLK_RETURN;
                        break;
                    case SDL_GAMEPAD_BUTTON_EAST: // B / Circle
                        menu_key = SDLK_END;
                        break;
                    default:
                        break;
                    }

                    if (menu_key != SDLK_UNKNOWN)
                    {
                        push_gamepad_menu_key(menu_key);
                    }
                }
                else if (player_controller_active && event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
                {
                    player_jump_requested = true;
                }
                else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_WEST)
                {
                    // Gamepad equivalent of P: toggle the player controller.
                    if (player_controller_active)
                    {
                        player_controller_active = false;
                        camera_follow_player = false;
                        std::println("Player controller disabled; world viewer camera restored.");
                    }
                    else if (!has_loaded_model)
                    {
                        std::println("Load a character model before enabling the player controller.");
                    }
                    else if (has_loaded_stage && !player_spawn_valid)
                    {
                        std::println(std::cerr,
                                     "Player control unavailable: this stage has no validated HIT spawn surface.");
                    }
                    else if (has_loaded_stage && !default_player_model_loaded)
                    {
                        std::println(std::cerr,
                                     "Player control unavailable: the default player model did not load.");
                    }
                    else
                    {
                        player_controller_active = true;
                        if (!has_loaded_stage)
                        {
                            player_controller.m_position = {
                                camera.m_target[0], player_controller.m_ground_height, camera.m_target[2]};
                            player_controller.m_velocity = {0.0f, 0.0f, 0.0f};
                            player_controller.m_fixed_time_accumulator = 0.0f;
                            player_controller.m_jump_pressed_pending = false;
                            player_controller.m_grounded = true;
                            player_actor_placed = true;
                        }
                        player_camera.m_follow_offset = {0.0f, 1.35f, 0.0f};
                        player_camera.m_target = {
                            player_controller.m_position[0] + player_camera.m_follow_offset[0],
                            player_controller.m_position[1] + player_camera.m_follow_offset[1],
                            player_controller.m_position[2] + player_camera.m_follow_offset[2]};
                        player_camera.m_framed = true;
                        camera_follow_player = true;
                        std::println(
                            "Player controller enabled: WASD / left stick move, Shift / LB run, Space / A "
                            "jump. Mouse orbits gameplay camera; wheel changes signed orbit distance.");
                    }
                }
                else if (player_controller_active && event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH)
                {
                    // Gamepad equivalent of R: unstuck / respawn at stage spawn.
                    place_player_on_stage();
                    std::println("Player unstuck: respawned at stage spawn");
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (gamepad && event.gbutton.which == SDL_GetGamepadID(gamepad))
                {
                    if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)
                    {
                        gamepad_select_down = false;
                    }
                    else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_START)
                    {
                        gamepad_start_down = false;
                    }
                    if (!gamepad_select_down && !gamepad_start_down)
                    {
                        gamepad_menu_chord_latched = false;
                    }
                }
                break;
            default:
                break;
            }
        }

        // Update player movement from keyboard and the first connected gamepad.
        if (player_controller_active)
        {
            sdbz::s_player_input player_input{};
            if (!debug_menu.open && !vfs_open)
            {
                const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
                if (keyboard_state)
                {
                    player_input.m_move_x = static_cast<float>(keyboard_state[SDL_SCANCODE_D]) -
                                            static_cast<float>(keyboard_state[SDL_SCANCODE_A]);
                    player_input.m_move_forward = static_cast<float>(keyboard_state[SDL_SCANCODE_W]) -
                                                  static_cast<float>(keyboard_state[SDL_SCANCODE_S]);
                    player_input.m_run = keyboard_state[SDL_SCANCODE_LSHIFT] || keyboard_state[SDL_SCANCODE_RSHIFT];
                }

                if (gamepad)
                {
                    player_input.m_move_x += gamepad_axis_to_input(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX));
                    player_input.m_move_forward -=
                        gamepad_axis_to_input(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
                    player_input.m_run =
                        player_input.m_run || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);

                    if (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
                    {
                        // Left bumper held increases the distance-change speed.
                        const float speed_multiplier =
                            SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) ? 12.0f : 4.0f;
                        player_camera.m_orbit_distance = std::clamp(
                            player_camera.m_orbit_distance +
                                gamepad_axis_to_input(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY)) *
                                    static_cast<float>(frame_time) * speed_multiplier,
                            -1000000.0f, 1000000.0f);
                    }
                    else
                    {
                        const float camera_input_scale = static_cast<float>(frame_time) * 60.0f;
                        sdbz::orbit_camera(player_camera,
                            gamepad_axis_to_input(-SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) *
                                camera_input_scale,
                            gamepad_axis_to_input(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY)) *
                                camera_input_scale);
                    }
                }
                player_input.m_jump_pressed = player_jump_requested;

                // Interpret movement relative to the camera's ground-plane axes.
                player_input = sdbz::make_camera_relative_player_input(
                    player_input, player_camera.m_yaw + (player_camera.m_orbit_distance < 0.0f ? 180.0f : 0.0f));
            }

            const sdbz::s_stage_collision_world* collision_world = nullptr;
            if (has_loaded_stage && player_spawn_valid && loaded_stage_index < stage_collision_worlds.size())
            {
                collision_world = &stage_collision_worlds[loaded_stage_index];
            }
            sdbz::update_player_controller(player_controller, player_input, static_cast<float>(frame_time),
                                           collision_world);

            // PMT clip selection is name-driven rather than index-driven. If a
            // package has no recognisable state names, keep its current preview
            // clip instead of guessing an animation index.
            if (player_controller_active && has_loaded_motion && !loaded_motion.m_motions.empty())
            {
                if (player_controller.m_landed_this_frame)
                {
                    const auto land_clip =
                        sdbz::find_player_animation_clip(loaded_motion.m_motions, sdbz::e_player_animation_state::land);
                    if (land_clip)
                    {
                        player_land_animation_remaining =
                            std::max(loaded_motion.m_motions[*land_clip].m_duration, 0.1f);
                    }
                }

                const bool landing_override = player_land_animation_remaining > 0.0f;
                const auto animation_state = sdbz::classify_player_animation_state(player_controller, landing_override);
                const auto selected_clip = sdbz::find_player_animation_clip(loaded_motion.m_motions, animation_state);
                if (selected_clip &&
                    (active_player_animation_state != animation_state || motion_clip_index != *selected_clip))
                {
                    motion_clip_index = *selected_clip;
                    animation_time = 0.0f;
                    active_player_animation_state = animation_state;
                }

                player_land_animation_remaining =
                    std::max(0.0f, player_land_animation_remaining - static_cast<float>(frame_time));
            }

            if (camera_follow_player)
            {
                sdbz::update_follow_camera(player_camera, player_controller.m_position, static_cast<float>(frame_time));
            }
        }

        // Advance animation time.
        if (has_loaded_motion && motion_clip_index < loaded_motion.m_motions.size())
        {
            animation_time += static_cast<float>(frame_time);
            const auto& clip = loaded_motion.m_motions[motion_clip_index];
            if (animation_time > clip.m_duration)
            {
                animation_time = 0.0f;
            }
        }
        ctx.animation_time = animation_time;

        // Update per-frame context fields. VFS selections can replace any of
        // these resources while the application is running.
        window.get_size_pixels(ctx.drawable_width, ctx.drawable_height);
        ctx.model = has_loaded_model ? &loaded_model : nullptr;
        ctx.has_model = has_loaded_model;
        ctx.mesh_visible = &mesh_visible;
        ctx.part_groups = &part_groups;
        ctx.motion = has_loaded_motion ? &loaded_motion : nullptr;
        ctx.has_motion = has_loaded_motion;
        ctx.image = has_loaded_image ? &loaded_image : nullptr;
        ctx.has_image = has_loaded_image;
        ctx.has_textures = has_textures;
        ctx.texture_names = &texture_names;
        ctx.stages = has_loaded_stage ? &loaded_stages : nullptr;
        ctx.has_font = has_font;
        ctx.font = has_font ? &font : nullptr;
        ctx.has_stage = has_loaded_stage;
        ctx.stage_index = loaded_stage_index;
        ctx.has_stage_model = has_stage_model;
        ctx.has_stage_hit = has_stage_hit;
        ctx.motion_clip_index = motion_clip_index;
        ctx.camera = player_controller_active ? &player_camera : &camera;
        ctx.viewer_camera = &camera;
        ctx.player_controller = &player_controller;
        ctx.stage_collision_world = has_loaded_stage && loaded_stage_index < stage_collision_worlds.size()
                                        ? &stage_collision_worlds[loaded_stage_index]
                                        : nullptr;
        ctx.player_controller_active = player_controller_active;
        ctx.player_actor_placed = player_actor_placed;
        ctx.show_effect = show_effect;
        ctx.show_collision = show_collision;
        ctx.show_templates = show_templates;

        // Build debug menu items after events so Home-opened menus show items.
        ctx.debug.open = debug_menu.open;
        ctx.debug.selected = debug_menu.selected;
        if (debug_menu.open && !vfs_open)
        {
            debug_menu_build(debug_menu, has_loaded_stage,
                             has_loaded_stage ? loaded_stages[loaded_stage_index].m_names[0].c_str() : nullptr,
                             has_loaded_motion,
                             has_loaded_motion ? loaded_motion.m_motions[motion_clip_index].m_name.c_str() : nullptr,
                             show_effect, show_collision, show_templates, has_loaded_archive);
            ctx.debug.items = debug_menu.items;
            ctx.debug.selected = debug_menu.selected;
            ctx.debug.stack = debug_menu.stack;
        }
        else
        {
            ctx.debug.items.clear();
        }

        // Build VFS entries after events so Enter-opened VFS shows items.
        ctx.vfs_open = vfs_open;
        ctx.vfs_selected = vfs_selected;
        ctx.vfs_archive = has_loaded_archive ? &loaded_archive : nullptr;
        ctx.vfs_game_data = has_game_data ? &game_data : nullptr;
        ctx.vfs_path_components = vfs_path_components;
        ctx.vfs_entries.clear();
        if (vfs_open && has_loaded_archive)
        {
            // Build the current path prefix string.
            std::string prefix;
            for (const auto& comp : vfs_path_components)
            {
                if (!prefix.empty())
                    prefix += "/";
                prefix += comp;
            }

            // Collect direct children of the current path.
            std::vector<std::pair<std::string, std::size_t>> dirs;
            std::vector<std::pair<std::string, std::size_t>> files;
            std::vector<std::string> seen_names;
            for (std::size_t i = 0; i < loaded_archive.m_entries.size(); ++i)
            {
                const auto& entry = loaded_archive.m_entries[i];
                // Normalize separators to forward slash for matching.
                std::string name = entry.m_name_cp932;
                std::replace(name.begin(), name.end(), '\\', '/');
                if (prefix.empty() || name.compare(0, prefix.size(), prefix) == 0)
                {
                    // The character right after the prefix must be a separator,
                    // otherwise dis/d00.pix matches prefix dis/d00 (false child).
                    if (!prefix.empty() && name.size() > prefix.size() && name[prefix.size()] != '/')
                    {
                        continue;
                    }
                    std::string rest = name.substr(prefix.size());
                    while (!rest.empty() && (rest[0] == '\\' || rest[0] == '/'))
                        rest.erase(rest.begin());
                    if (rest.empty())
                        continue;
                    const std::size_t sep = rest.find_first_of("\\/");
                    const std::string child = rest.substr(0, sep);
                    if (std::find(seen_names.begin(), seen_names.end(), child) != seen_names.end())
                        continue;
                    seen_names.push_back(child);
                    const bool is_dir = (sep != std::string::npos) || entry.m_is_directory;
                    if (is_dir)
                        dirs.push_back({child, i});
                    else
                        files.push_back({child, i});
                }
            }
            std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& d : dirs)
                ctx.vfs_entries.push_back({d.second, d.first, true});
            for (const auto& f : files)
                ctx.vfs_entries.push_back({f.second, f.first, false});

            if (vfs_selected < 0 || vfs_selected >= static_cast<int>(ctx.vfs_entries.size()))
            {
                vfs_selected = 0;
                ctx.vfs_selected = 0;
            }
        }

        render_frame(ctx);
    }

    // Shutdown.
    for (const auto tex : texture_names)
    {
        if (tex)
            glDeleteTextures(1, &tex);
    }
    if (gamepad)
    {
        SDL_CloseGamepad(gamepad);
    }
    window.destroy();
    SDL_Quit();
    return 0;
}
