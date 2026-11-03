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
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <limits>

#include "assets/assets.hpp"
#include "core/animation.hpp"
#include "core/math.hpp"
#include "core/model.hpp"
#include "core/stage.hpp"
#include "core/texture.hpp"

#include "audio/adpcm.hpp"
#include "core/ase.hpp"

#include "render/renderer.hpp"

namespace fs = std::filesystem;

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
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--mesh" && i + 1 < argc)
        {
            mesh_filters.push_back(argv[++i]);
            continue;
        }
        if (arg == "--info" && i + 1 < argc)
        {
            info_path = argv[++i];
            continue;
        }
        if (arg == "--game" && i + 1 < argc)
        {
            game_path = argv[++i];
            continue;
        }
        // First non-flag argument is the input model/path.
        if (input_path == "assets/extracted")
        {
            input_path = arg;
            std::cout << "Input: " << input_path.string() << '\n';
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
    if (input_path == "assets/extracted")
    {
        std::cout << "Input: " << input_path.string() << '\n';
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
    std::vector<std::vector<GLuint>> stage_mesh_lists;
    bool show_effect = false;
    bool show_templates = false;
    bool show_collision = false;
    bool has_stage_model = false;
    bool has_stage_hit = false;

    // FFON font.
    s_ffon_font font;
    bool has_font = false;
    std::vector<GLuint> glyph_textures;

    // Archive state (INFO.DAT / GAME.DAT).
    sdbz::s_archive loaded_archive;
    bool has_loaded_archive = false;
    std::vector<std::uint8_t> game_data;
    bool has_game_data = false;


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
                // Load ALL .stg files in the same folder (e.g. s03a.stg, s03b.stg).
                std::vector<fs::path> stage_files = list_files(
                    directory_of(input_path), ".stg");
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
                        std::cout << "Loaded stage: " << stg_path.filename().string()
                            << " (" << loaded_stages.back().m_nodes.size() << " nodes)\n";
                    }
                    catch (const std::exception& exception)
                    {
                        std::cerr << "Stage load failed for " << stg_path.filename().string()
                            << ": " << exception.what() << '\n';
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
                        const auto sibling = [&](const char* extension) -> fs::path
                        {
                            const std::string stem =
                                lower_copy(stg_path.stem().string());
                            for (const auto& candidate :
                                list_files(directory_of(stg_path), extension))
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
                                stage_models[si] = sdbz::parse_fmdl(
                                    sdbz::read_file(model_file), model_file.stem().string());
                                stage_models_loaded[si] = true;
                                has_stage_model = true;
                                std::cout << "Stage model " << model_file.filename().string()
                                    << ": " << stage_models[si].m_meshes.size() << " meshes\n";
                                for (std::size_t mi = 0; mi < stage_models[si].m_meshes.size(); ++mi)
                                {
                                    stage_meshes_by_names[si][
                                        stage_models[si].m_meshes[mi].m_name].push_back(mi);
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
                                    stage_hit_by_names[si][
                                        stage_hits[si].m_objects[oi].m_name] = oi;
                                }
                                std::cout << "Stage collision " << hit_file.filename().string()
                                    << ": " << stage_hits[si].m_objects.size() << " objects\n";
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
                        for (std::size_t ni = 0; ni < nodes.size(); ++ni)
                        {
                            const auto& node = nodes[ni];
                            const sdbz::mat4 local = sdbz::trs_matrix(
                                node.m_translation, node.m_rotation_radians, node.m_scale);
                            stage_worlds[si][ni] = (node.m_parent >= 0 &&
                                static_cast<std::size_t>(node.m_parent) < ni)
                                ? sdbz::mat4_mul(stage_worlds[si][
                                    static_cast<std::size_t>(node.m_parent)], local)
                                : local;
                            std::size_t root = ni;
                            while (nodes[root].m_parent >= 0)
                                root = static_cast<std::size_t>(nodes[root].m_parent);
                            stage_groups[si][ni] = root == loaded_stages[si].m_stage_root ? 0 :
                                (root == loaded_stages[si].m_effect_root ? 1 : 2);
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
        font_path = find_font_near(is_mdl || is_pix || is_hit || is_stg
            ? directory_of(input_path) : input_path);
        if (!font_path.empty()) std::cout << "Auto font: " << font_path.string() << '\n';
    }

    // Load every motion package and merge their clips into one list.
    for (const auto& path : motion_paths)
    {
        try
        {
            auto package = sdbz::parse_rfsm(sdbz::read_file(path));
            std::cout << "Loaded motion package " << path.filename().string() << ": "
                << package.m_motions.size() << " motions, " << package.m_bone_names.size()
                << " bones\n";
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
            std::cerr << "Motion load unavailable (" << path.filename().string() << "): "
                << exception.what() << '\n';
            if (explicit_motion) return 2;
        }
    }
    if (has_loaded_motion && loaded_motion.m_motions.empty()) has_loaded_motion = false;

    // Auto-detect INFO.DAT/GAME.DAT next to the input path if not explicitly provided.
    if (!has_loaded_archive && !info_path.empty() && !game_path.empty() &&
        fs::is_regular_file(info_path) && fs::is_regular_file(game_path))
    {
        try
        {
            const auto info_data = sdbz::read_file(info_path);
            loaded_archive = sdbz::s_archive::from_encoded_info(info_data);
            has_loaded_archive = true;
            game_data = sdbz::read_file(game_path);
            has_game_data = true;
    std::cout << "Loaded archive: " << info_path.filename().string()
        << " (" << loaded_archive.m_entries.size() << " entries) + "
        << game_path.filename().string() << " (" << game_data.size() << " bytes)\n";
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Archive load unavailable: " << exception.what() << '\n';
            has_loaded_archive = false;
            has_game_data = false;
        }
    }

    if (!has_loaded_archive)
    {
        const fs::path input_dir = fs::is_regular_file(input_path)
            ? input_path.parent_path()
            : input_path;
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
                std::cout << "Auto-loaded archive: " << auto_info.filename().string()
                    << " (" << loaded_archive.m_entries.size() << " entries) + "
                    << auto_game.filename().string() << " (" << game_data.size() << " bytes)\n";
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
                    std::cout << "Loaded archive fallback: " << fallback_info.string()
                        << " (" << loaded_archive.m_entries.size() << " entries) + "
                        << fallback_game.string() << " (" << game_data.size() << " bytes)\n";
                    break;
                }
                catch (const std::exception& exception)
                {
                    std::cerr << "Archive auto-load fallback unavailable: " << exception.what() << '\n';
                    has_loaded_archive = false;
                    has_game_data = false;
                }
            }
        }
    }

    // ---- SDL / OpenGL / audio platform initialisation ----
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_AUDIO);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("Super Dragon Ball Z - Native Rebuild (OpenGL)",
        1120, 490, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

    // Open audio device.
    SDL_AudioSpec audio_spec{};
    audio_spec.freq = 44100;
    audio_spec.format = SDL_AUDIO_S16;
    audio_spec.channels = 1;
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec);
    if (!audio_device)
    {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << '\n';
    }
    else
    {
        std::cout << "Audio device opened: " << audio_spec.freq << " Hz, "
            << audio_spec.channels << " channels\n";
    }

    // Load ASE sound effects from extracted assets (kept available, not played).
    std::vector<std::int16_t> attack_sound;
    const fs::path assets_extracted = "assets/extracted";
    if (fs::is_directory(assets_extracted))
    {
        for (const auto& entry : fs::recursive_directory_iterator(assets_extracted))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".ase")
            {
                try
                {
                    const auto ase_data = sdbz::read_file(entry.path());
                    const auto ase = sdbz::parse_ase(ase_data);
                    for (const auto& sample : ase.m_samples)
                    {
                        if (!sample.m_payload.empty())
                        {
                            sdbz::s_adpcm_decoder decoder;
                            const auto decoded = decoder.decode(sample.m_payload);
                            attack_sound.insert(attack_sound.end(),
                                decoded.begin(), decoded.end());
                        }
                    }
                    std::cout << "Loaded ASE: " << entry.path().filename().string()
                        << " (" << ase.m_samples.size() << " samples)\n";
                }
                catch (const std::exception& e)
                {
                    std::cerr << "ASE load failed for " << entry.path().filename().string()
                        << ": " << e.what() << '\n';
                }
            }
        }
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
                std::cout << "Loaded " << texture_names.size() << " textures from "
                    << texture_path.string() << '\n';
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Texture load unavailable: " << exception.what() << '\n';
            }
        }
    }

    // Load FFON font AFTER OpenGL context is created.
    if (!font_path.empty() && fs::is_regular_file(font_path))
    {
        if (has_extension(font_path, ".fon"))
        {
            try
            {
                has_font = load_ffon(font_path, font);
                if (has_font)
                {
                    upload_glyph_textures(font, glyph_textures);
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
                std::cout << "Loaded stage textures "
                    << stage_pix_paths[si].filename().string() << ": "
                    << stage_textures[si].size() << " textures\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Stage texture load failed: " << exception.what() << '\n';
            }
        }
        else if (has_loaded_archive &&
            stage_pix_entry_indices[si] != std::numeric_limits<std::size_t>::max() &&
            stage_pix_entry_indices[si] < loaded_archive.m_entries.size())
        {
            try
            {
                const auto& entry = loaded_archive.m_entries[
                    stage_pix_entry_indices[si]];
                const std::uint64_t offset = entry.offset();
                const std::uint64_t byte_size = entry.m_byte_size;
                if (offset + byte_size <= game_data.size())
                {
                    const std::span<const std::uint8_t> entry_data(
                        game_data.data() + offset, byte_size);
                    stage_textures[si] = load_fpix_textures(
                        std::vector<std::uint8_t>(
                            entry_data.begin(), entry_data.end()));
                    stage_textures_loaded[si] = !stage_textures[si].empty();
                    std::cout << "Loaded stage textures from archive: "
                        << entry.m_name_cp932 << ": "
                        << stage_textures[si].size() << " textures\n";
                }
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Archive stage texture load failed: "
                    << exception.what() << '\n';
            }
        }
    }

    // ---- Viewer context + camera ----
    s_camera camera;
    sdbz::s_debug_menu debug_menu;
    bool vfs_open = false;
    int vfs_selected = 0;
    std::vector<std::string> vfs_path_components;
    s_draw_context ctx;
    ctx.window = window;
    ctx.camera = &camera;
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

    auto previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

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
                    if (debug_menu_input(debug_menu, event, running,
                        show_effect, show_collision, show_templates,
                        camera.m_framed, camera.m_yaw, camera.m_pitch,
                        vfs_open, vfs_path_components, vfs_selected,
                        has_loaded_archive,
                        has_loaded_stage, loaded_stage_index, loaded_stages,
                        has_loaded_motion, motion_clip_index,
                        loaded_motion.m_motions, animation_time))
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
                        if (count == 0) break;
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
                            if (count == 0 || !has_game_data) break;
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
                                // Load file based on extension.
                                const std::uint64_t offset = entry.offset();
                                const std::uint64_t byte_size = entry.m_byte_size;
                                if (offset + byte_size > game_data.size()) break;
                                const std::span<const std::uint8_t> entry_data(
                                    game_data.data() + offset, byte_size);
                                const std::string ext = fs::path(name).extension().string();
                            try
                            {
                                if (ext == ".mdl" || ext == ".MDL")
                                {
                                    loaded_model = sdbz::parse_fmdl(
                                        entry_data, fs::path(name).stem().string());
                                    has_loaded_model = true;
                                    mesh_visible.assign(loaded_model.m_meshes.size(), false);
                                    part_groups.clear();
                                    for (std::size_t mi = 0;
                                        mi < loaded_model.m_meshes.size(); ++mi)
                                    {
                                        const auto& mesh_name =
                                            loaded_model.m_meshes[mi].m_name;
                                        const std::string label =
                                            part_base_name(mesh_name);
                                        auto it = std::find_if(part_groups.begin(),
                                            part_groups.end(),
                                            [&](const s_part_group& g)
                                            { return g.m_label == label; });
                                        if (it == part_groups.end())
                                        {
                                            part_groups.push_back({ label, {}, 0 });
                                            it = part_groups.end() - 1;
                                        }
                                        it->m_meshes.push_back(mi);
                                    }
                                    apply_part_visibility(part_groups, mesh_visible);
                                    std::cout << "Loaded model from archive: "
                                        << loaded_model.m_name << '\n';
                                }
                                else if (ext == ".pix" || ext == ".PIX")
                                {
                                    texture_names = load_fpix_textures(
                                        std::vector<std::uint8_t>(
                                            entry_data.begin(), entry_data.end()));
                                    has_textures = !texture_names.empty();
                                    std::cout << "Loaded textures from archive: "
                                        << texture_names.size() << '\n';
                                }
                                else if (ext == ".fon" || ext == ".FON")
                                {
                                    // Write font to temp file for load_ffon (takes a path).
                                    const fs::path temp_font =
                                        fs::temp_directory_path() / "sdbz_vfs_font.fon";
                                    sdbz::write_file(temp_font, entry_data);
                                    has_font = load_ffon(temp_font, font);
                                    if (has_font)
                                    {
                                        upload_glyph_textures(font, glyph_textures);
                                        std::cout << "Loaded font from archive: "
                                            << glyph_textures.size() << " glyphs\n";
                                    }
                                }
                                else if (ext == ".pmt" || ext == ".mot")
                                {
                                    auto package = sdbz::parse_rfsm(entry_data);
                                    loaded_motion = std::move(package);
                                    has_loaded_motion = true;
                                    motion_clip_index = 0;
                                    animation_time = 0.0f;
                                    std::cout << "Loaded motions from archive: "
                                        << loaded_motion.m_motions.size() << '\n';
                                }
                                else if (ext == ".stg" || ext == ".STG")
                                {
                                    try
                                    {
                                        const std::string stage_stem =
                                            fs::path(name).stem().string();
                                        sdbz::s_stage stg = sdbz::parse_stg(entry_data);

                                        // Find companion files in the archive by stem name.
                                        auto find_archive_entry = [&](const char* extension)
                                            -> const sdbz::s_archive_entry*
                                        {
                                            const std::string target =
                                                lower_copy(stage_stem + extension);
                                            for (const auto& candidate :
                                                loaded_archive.m_entries)
                                            {
                                                if (lower_copy(
                                                    candidate.m_name_cp932) == target)
                                                {
                                                    return &candidate;
                                                }
                                            }
                                            return nullptr;
                                        };

                                        // Stage model (.mdl).
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
                                                    sdbz::parse_fmdl(
                                                        mdata, model_entry->m_name_cp932));
                                                stage_models_loaded.push_back(true);
                                                has_stage_model = true;
                                                std::size_t mi =
                                                    stage_models.size() - 1;
                                                for (std::size_t mxi = 0;
                                                    mxi < stage_models[mi].m_meshes.size();
                                                    ++mxi)
                                                {
                                                    stage_meshes_by_names.back()[
                                                        stage_models[mi].m_meshes[mxi].m_name
                                                    ].push_back(mxi);
                                                }
                                                std::cout << "Stage model from archive: "
                                                    << model_entry->m_name_cp932 << " ("
                                                    << stage_models.back().m_meshes.size()
                                                    << " meshes)\n";
                                            }
                                        }
                                        else
                                        {
                                            std::cerr << "No " << stage_stem
                                                << ".mdl in archive; showing node hierarchy only\n";
                                            stage_models.emplace_back();
                                            stage_models_loaded.push_back(false);
                                        }

                                        // Collision (.hit).
                                        const auto* hit_entry = find_archive_entry(".hit");
                                        if (hit_entry)
                                        {
                                            const std::uint64_t hoffset = hit_entry->offset();
                                            const std::uint64_t hsize = hit_entry->m_byte_size;
                                            if (hoffset + hsize <= game_data.size())
                                            {
                                                const std::span<const std::uint8_t> hdata(
                                                    game_data.data() + hoffset, hsize);
                                                stage_hits.push_back(sdbz::parse_hit(hdata));
                                                stage_hits_loaded.push_back(true);
                                                has_stage_hit = true;
                                                for (std::size_t oi = 0;
                                                    oi < stage_hits.back().m_objects.size();
                                                    ++oi)
                                                {
                                                    stage_hit_by_names.back()[
                                                        stage_hits.back().m_objects[oi].m_name
                                                    ] = oi;
                                                }
                                                std::cout << "Stage collision from archive: "
                                                    << hit_entry->m_name_cp932 << " ("
                                                    << stage_hits.back().m_objects.size()
                                                    << " objects)\n";
                                            }
                                        }
                                        else
                                        {
                                            stage_hits.emplace_back();
                                            stage_hits_loaded.push_back(false);
                                        }

                                        // Stage textures (.pix) - loaded lazily after GL context.
                                        const auto* pix_entry = find_archive_entry(".pix");
                                        stage_pix_paths.push_back(fs::path());
                                        stage_textures.emplace_back();
                                        stage_textures_loaded.push_back(false);
                                        if (pix_entry)
                                        {
                                            const std::size_t pix_index =
                                                static_cast<std::size_t>(
                                                    pix_entry - loaded_archive.m_entries.data());
                                            stage_pix_entry_indices.push_back(pix_index);
                                        }
                                        else
                                        {
                                            stage_pix_entry_indices.push_back(
                                                std::numeric_limits<std::size_t>::max());
                                        }

                                        loaded_stages.push_back(std::move(stg));
                                        stage_worlds.emplace_back();
                                        stage_groups.emplace_back();
                                        stage_meshes_by_names.emplace_back();
                                        stage_hit_by_names.emplace_back();
                                        stage_mesh_lists.emplace_back();

                                        // Compute world transforms for the new stage.
                                        const std::size_t si = loaded_stages.size() - 1;
                                        const auto& nodes = loaded_stages[si].m_nodes;
                                        stage_worlds[si].resize(nodes.size());
                                        stage_groups[si].assign(nodes.size(), 2);
                                        for (std::size_t ni = 0; ni < nodes.size(); ++ni)
                                        {
                                            const auto& node = nodes[ni];
                                            const sdbz::mat4 local = sdbz::trs_matrix(
                                                node.m_translation, node.m_rotation_radians,
                                                node.m_scale);
                                            stage_worlds[si][ni] = (node.m_parent >= 0 &&
                                                static_cast<std::size_t>(node.m_parent) < ni)
                                                ? sdbz::mat4_mul(stage_worlds[si][
                                                    static_cast<std::size_t>(node.m_parent)],
                                                    local)
                                                : local;
                                            std::size_t root = ni;
                                            while (nodes[root].m_parent >= 0)
                                                root = static_cast<std::size_t>(
                                                    nodes[root].m_parent);
                                            stage_groups[si][ni] = root ==
                                                loaded_stages[si].m_stage_root ? 0 :
                                                (root == loaded_stages[si].m_effect_root ? 1 : 2);
                                        }

                                        has_loaded_stage = true;
                                        loaded_stage_index = loaded_stages.size() - 1;
                                        std::cout << "Loaded stage from archive: " << name
                                            << " (" << loaded_stages.back().m_nodes.size()
                                            << " nodes)\n";
                                    }
                                    catch (const std::exception& exception)
                                    {
                                        std::cerr << "Stage load from archive failed ("
                                            << name << "): " << exception.what() << '\n';
                                    }
                                }
                                else if (ext == ".hit" || ext == ".HIT")
                                {
                                    loaded_hit = sdbz::parse_hit(entry_data);
                                    has_loaded_hit = true;
                                    std::cout << "Loaded hit from archive: "
                                        << loaded_hit.m_objects.size() << " objects\n";
                                }
                                else
                                {
                                    std::cout << "Archive entry not loadable: " << name << '\n';
                                }
                            }
                            catch (const std::exception& exception)
                            {
                                std::cerr << "Archive entry load failed (" << name << "): "
                                    << exception.what() << '\n';
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
                        const std::size_t gi =
                            static_cast<std::size_t>(event.key.key - SDLK_1);
                        if (gi < part_groups.size())
                        {
                            auto& group = part_groups[gi];
                            const std::size_t states =
                                group.m_meshes.size() + 1; // variants + hidden
                            group.m_active = backwards ? (group.m_active + states - 1) % states
                                : (group.m_active + 1) % states;
                            apply_part_visibility(part_groups, mesh_visible);
                            std::cout << group.m_label << ": "
                                << (group.m_active < group.m_meshes.size()
                                    ? loaded_model.m_meshes[
                                        group.m_meshes[group.m_active]].m_name
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
                // Stage viewer toggles.
                if (has_loaded_stage)
                {
                    if (event.key.key == SDLK_E) show_effect = !show_effect;
                    else if (event.key.key == SDLK_C) show_collision = !show_collision;
                    else if (event.key.key == SDLK_T) show_templates = !show_templates;
                    else if (event.key.key == SDLK_PAGEUP)
                    {
                        loaded_stage_index =
                            (loaded_stage_index + loaded_stages.size() - 1) %
                            loaded_stages.size();
                        std::cout << "Stage: "
                            << loaded_stages[loaded_stage_index].m_names[0] << '\n';
                    }
                    else if (event.key.key == SDLK_PAGEDOWN)
                    {
                        loaded_stage_index =
                            (loaded_stage_index + 1) % loaded_stages.size();
                        std::cout << "Stage: "
                            << loaded_stages[loaded_stage_index].m_names[0] << '\n';
                    }
                }
                // Camera zoom and re-fit.
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
                    camera.m_framed = false;
                    camera.m_yaw = 0.0f;
                    camera.m_pitch = 0.0f;
                }
                // Motion clip cycling.
                if (has_loaded_motion)
                {
                    if (event.key.key == SDLK_N)
                    {
                        motion_clip_index =
                            (motion_clip_index + 1) % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        std::cout << "Motion clip " << motion_clip_index << ": "
                            << loaded_motion.m_motions[motion_clip_index].m_name << '\n';
                    }
                    else if (event.key.key == SDLK_B)
                    {
                        motion_clip_index =
                            (motion_clip_index + loaded_motion.m_motions.size() - 1)
                            % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        std::cout << "Motion clip " << motion_clip_index << ": "
                            << loaded_motion.m_motions[motion_clip_index].m_name << '\n';
                    }
                }
                break;
            case SDL_EVENT_KEY_UP:
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_button_down = true;
                    last_mouse_x = static_cast<int>(event.button.x);
                    last_mouse_y = static_cast<int>(event.button.y);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT ||
                    event.button.button == SDL_BUTTON_MIDDLE)
                {
                    pan_button_down = true;
                    last_mouse_x = static_cast<int>(event.button.x);
                    last_mouse_y = static_cast<int>(event.button.y);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_button_down = false;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT ||
                    event.button.button == SDL_BUTTON_MIDDLE)
                {
                    pan_button_down = false;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (left_button_down)
                {
                    const int dx = static_cast<int>(event.motion.x) - last_mouse_x;
                    const int dy = static_cast<int>(event.motion.y) - last_mouse_y;
                    // Flip a sign to invert that axis of the mouse drag.
                    constexpr float k_drag_x = -0.5f;
                    constexpr float k_drag_y = -0.5f;
                    camera.m_yaw += dx * k_drag_x;
                    camera.m_pitch = std::clamp(camera.m_pitch + dy * k_drag_y, -85.0f, 85.0f);
                    last_mouse_x = static_cast<int>(event.motion.x);
                    last_mouse_y = static_cast<int>(event.motion.y);
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
                camera.m_distance = std::clamp(camera.m_distance * std::pow(0.88f, static_cast<float>(event.wheel.y)), 0.05f, 1.0e6f);
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
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                break;
            default:
                break;
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

        // Update per-frame context fields.
        SDL_GetWindowSizeInPixels(window, &ctx.drawable_width, &ctx.drawable_height);
        ctx.stage_index = loaded_stage_index;
        ctx.motion_clip_index = motion_clip_index;
        ctx.show_effect = show_effect;
        ctx.show_collision = show_collision;
        ctx.show_templates = show_templates;

        // Build debug menu items after events so Home-opened menus show items.
        ctx.debug.open = debug_menu.open;
        ctx.debug.selected = debug_menu.selected;
        if (debug_menu.open && !vfs_open)
        {
            debug_menu_build(debug_menu,
                has_loaded_stage,
                has_loaded_stage ? loaded_stages[loaded_stage_index].m_names[0].c_str() : nullptr,
                has_loaded_motion,
                has_loaded_motion ? loaded_motion.m_motions[motion_clip_index].m_name.c_str() : nullptr,
                show_effect, show_collision, show_templates,
                has_loaded_archive);
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
                if (!prefix.empty()) prefix += "/";
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
                    if (!prefix.empty() && name.size() > prefix.size() &&
                        name[prefix.size()] != '/')
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
                        dirs.push_back({ child, i });
                    else
                        files.push_back({ child, i });
                }
            }
            std::sort(dirs.begin(), dirs.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            std::sort(files.begin(), files.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& d : dirs)
                ctx.vfs_entries.push_back({ d.second, d.first, true });
            for (const auto& f : files)
                ctx.vfs_entries.push_back({ f.second, f.first, false });

            if (vfs_selected < 0 ||
                vfs_selected >= static_cast<int>(ctx.vfs_entries.size()))
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

