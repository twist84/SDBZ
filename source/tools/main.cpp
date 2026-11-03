#include "core/animation.hpp"
#include "core/binary_reader.hpp"
#include "core/container.hpp"
#include "core/elf.hpp"
#include "core/info_archive.hpp"
#include "core/model.hpp"
#include "core/texture.hpp"
#include "core/stage.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void print_usage()
    {
        std::cout <<
            "Super Dragon Ball Z native tools\n"
            "\n"
            "Usage:\n"
            "  sdbz_tool elf <SLUS_214.42>\n"
            "  sdbz_tool info <INFO.DAT> [GAME.DAT]\n"
            "  sdbz_tool container <asset.mdl|asset.pix>\n"
            "  sdbz_tool texture <asset.pix> <output.bmp>\n"
            "  sdbz_tool model <asset.mdl>\n"
            "  sdbz_tool model-obj <asset.mdl> <output.obj>\n"
            "  sdbz_tool motion <asset.pmt|asset.mot>\n"
            "  sdbz_tool stage <asset.stg>\n"
            "  sdbz_tool hit <asset.hit>\n"
            "  sdbz_tool hit-obj <asset.hit> <output.obj>\n"
            "  sdbz_tool extract <INFO.DAT> <GAME.DAT> <output_dir> [--overwrite]\n"
            "  sdbz_tool rebuild <INFO.DAT> <GAME.DAT> <replacement_dir|-> <output_dir>\n";
    }

    [[nodiscard]] std::string printable_name(const std::string& value)
    {
        std::ostringstream output;
        for (const unsigned char ch : value)
        {
            if (ch >= 0x20 && ch <= 0x7E && ch != '\\')
            {
                output << static_cast<char>(ch);
            }
            else
            {
                output << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(ch) << std::dec;
            }
        }
        return output.str();
    }

    int command_elf(const std::filesystem::path& path)
    {
        const auto data = sdbz::read_file(path);
        const auto elf = sdbz::inspect_elf32(data);

        std::cout << "File:                 " << path.string() << '\n'
                  << "Size:                 " << data.size() << " bytes\n"
                  << "Class:                ELF32\n"
                  << "Data:                 little-endian\n"
                  << "Type:                 " << (elf.m_type == 2 ? "executable" : std::to_string(elf.m_type)) << '\n'
                  << "Machine:              " << sdbz::elf_machine_name(elf.m_machine) << '\n'
                  << "Entry point:          0x" << std::hex << elf.m_entry << std::dec << '\n'
                  << "ELF flags:            0x" << std::hex << elf.m_flags << std::dec << '\n'
                  << "Program headers:      " << elf.m_program_header_count << '\n'
                  << "Section headers:      " << elf.m_section_header_count << '\n';

        if (elf.m_machine == 8 && (elf.m_flags & 0xF0000000u) == 0x20000000u)
        {
            std::cout << "PS2 hint:             MIPS-III / R5900 flag family\n";
        }
        return 0;
    }

    int command_info(const std::filesystem::path& info_path, const std::filesystem::path* game_path)
    {
        const auto encoded = sdbz::read_file(info_path);
        std::uint64_t game_size = 0;
        if (game_path)
        {
            game_size = std::filesystem::file_size(*game_path);
        }
        const auto archive = sdbz::s_archive::from_encoded_info(encoded, game_size);

        std::cout << "Entries:              " << archive.m_entries.size() << '\n'
                  << "Indexed bytes:        " << archive.m_used_bytes << '\n'
                  << "INFO.DAT size:        " << archive.m_encoded_info.size() << '\n';
        if (game_path)
        {
            std::cout << "GAME.DAT size:        " << game_size << '\n';
        }

        std::cout << "\n"
                  << std::left << std::setw(7) << "Index"
                  << std::setw(14) << "Sector"
                  << std::setw(14) << "Size"
                  << "Name\n";
        for (const auto& entry : archive.m_entries)
        {
            std::cout << std::left << std::setw(7) << entry.m_index
                      << std::setw(14) << entry.m_sector_offset
                      << std::setw(14) << entry.m_byte_size
                      << printable_name(entry.m_name_cp932) << '\n';
        }
        return 0;
    }

    int command_container(const std::filesystem::path& path)
    {
        const auto data = sdbz::read_file(path);
        const auto container = sdbz::parse_chunked_container(data);
        std::cout << "Container:            " << std::string(container.m_magic.begin(), container.m_magic.end()) << '\n'
                  << "File size:            " << container.m_file_size << '\n'
                  << "Chunk count:          " << container.m_chunk_count << '\n'
                  << "Format:               0x" << std::hex << container.m_format << std::dec << '\n';

        for (std::size_t index = 0; index < container.m_chunks.size(); ++index)
        {
            const auto& chunk = container.m_chunks[index];
            std::cout << "  [" << std::setw(4) << std::setfill('0') << index << "] "
                      << chunk.magic_string()
                      << " offset=0x" << std::hex << chunk.m_offset
                      << " size=0x" << chunk.m_size << std::dec << std::setfill(' ') << '\n';
        }
        return 0;
    }

    int command_texture(const std::filesystem::path& input_path, const std::filesystem::path& output_path)
    {
        const auto data = sdbz::read_file(input_path);
        const auto image = sdbz::decode_first_fpix_texture(data);
        sdbz::write_bmp_rgba(output_path, image);
        std::cout << "Decoded " << image.width << 'x' << image.height << " texture to " << output_path.string() << '\n';
        return 0;
    }

    int command_model(const std::filesystem::path& path)
    {
        const auto data = sdbz::read_file(path);
        const auto model = sdbz::parse_fmdl(data, path.stem().string());

        std::size_t total_vertices = 0;
        std::size_t total_triangles = 0;
        std::size_t skinned_meshes = 0;
        std::size_t morph_meshes = 0;
        for (const auto& mesh : model.m_meshes)
        {
            total_vertices += mesh.m_vertices.size();
            if (mesh.m_is_skinned)
            {
                ++skinned_meshes;
            }
            if (mesh.m_has_morph)
            {
                ++morph_meshes;
            }
            for (const auto& primitive : mesh.m_primitives)
            {
                total_triangles += primitive.m_indices.size() / 3;
            }
        }

        std::cout << "Model:                " << model.m_name << '\n'
                  << "File size:            " << data.size() << " bytes\n"
                  << "Hierarchy nodes:      " << model.m_nodes.size() << '\n'
                  << "Materials:            " << model.m_materials.size() << '\n'
                  << "Meshes:               " << model.m_meshes.size() << '\n'
                  << "Vertices:             " << total_vertices << '\n'
                  << "Triangles:            " << total_triangles << '\n'
                  << "Skinned meshes:       " << skinned_meshes << '\n'
                  << "Morph meshes:         " << morph_meshes << '\n';

        std::cout << "\nMeshes\n"
                  << std::left << std::setw(5) << "#"
                  << std::setw(28) << "Name"
                  << std::setw(12) << "Vertices"
                  << std::setw(12) << "Triangles"
                  << "Flags\n";
        for (std::size_t index = 0; index < model.m_meshes.size(); ++index)
        {
            const auto& mesh = model.m_meshes[index];
            std::size_t triangles = 0;
            for (const auto& primitive : mesh.m_primitives)
            {
                triangles += primitive.m_indices.size() / 3;
            }
            std::cout << std::left << std::setw(5) << index
                      << std::setw(28) << printable_name(mesh.m_name)
                      << std::setw(12) << mesh.m_vertices.size()
                      << std::setw(12) << triangles
                      << "0x" << std::hex << mesh.m_vertex_flags << std::dec;
            if (mesh.m_is_skinned)
            {
                std::cout << " skinned";
            }
            if (mesh.m_has_morph)
            {
                std::cout << " morph";
            }
            std::cout << '\n';
        }

        return 0;
    }

    int command_rebuild(
        const std::filesystem::path& info_path,
        const std::filesystem::path& game_path,
        const std::filesystem::path& replacement_path,
        const std::filesystem::path& output_path)
    {
        const auto encoded = sdbz::read_file(info_path);
        const auto game_size = std::filesystem::file_size(game_path);
        const auto archive = sdbz::s_archive::from_encoded_info(encoded, game_size);
        const auto replacement_root = replacement_path == "-" ? std::filesystem::path{} : replacement_path;
        archive.rebuild(info_path, game_path, replacement_root, output_path);
        std::cout << "Rebuilt archive with " << archive.m_entries.size() << " indexed assets in "
                  << output_path.string() << '\n';
        return 0;
    }

    void print_motion_clip(const sdbz::s_animation_clip& clip, const std::size_t index)
    {
        std::array<std::size_t, 3> animated_nodes{};
        for (const auto& node : clip.m_nodes)
        {
            animated_nodes[0] += node.m_translation.m_is_animated ? 1u : 0u;
            animated_nodes[1] += node.m_rotation.m_is_animated ? 1u : 0u;
            animated_nodes[2] += node.m_scale.m_is_animated ? 1u : 0u;
        }
        std::cout << "  [" << std::setw(3) << index << "] " << printable_name(clip.m_name)
                  << " frames=" << clip.m_frame_count
                  << " fps=" << clip.m_frames_per_second
                  << " nodes=" << clip.m_nodes.size()
                  << " channels(T/R/S)=" << clip.m_channel_counts[0] << '/'
                  << clip.m_channel_counts[1] << '/' << clip.m_channel_counts[2]
                  << " animated-nodes=" << animated_nodes[0] << '/'
                  << animated_nodes[1] << '/' << animated_nodes[2] << '\n';
    }

    int command_motion(const std::filesystem::path& path)
    {
        const auto data = sdbz::read_file(path);
        if (data.size() < 4)
        {
            throw std::runtime_error("motion file is too small");
        }
        const auto magic = sdbz::fourcc_to_string(data.data());
        if (magic == "MOT2")
        {
            const auto clip = sdbz::parse_mot2(data, path.stem().string());
            std::cout << "MOT2: " << printable_name(clip.m_name) << '\n'
                      << "FPS:                  " << clip.m_frames_per_second << '\n'
                      << "Frame count:          " << clip.m_frame_count << '\n'
                      << "Duration:             " << clip.m_duration << " seconds\n"
                      << "Last frame time:      " << clip.m_last_frame_time << " seconds\n"
                      << "Nodes:                " << clip.m_nodes.size() << '\n'
                      << "Channels T/R/S:       " << clip.m_channel_counts[0] << '/'
                      << clip.m_channel_counts[1] << '/' << clip.m_channel_counts[2] << '\n';
            return 0;
        }

        const auto package = sdbz::parse_rfsm(data);
        std::cout << "RFSM/L3 motion package\n"
                  << "Bone names:           " << package.m_bone_names.size() << '\n'
                  << "Motions:              " << package.m_motions.size() << '\n';
        for (std::size_t index = 0; index < package.m_motions.size(); ++index)
        {
            print_motion_clip(package.m_motions[index], index);
        }
        return 0;
    }


    int command_stage(const std::filesystem::path& path)
    {
        const auto data = sdbz::read_file(path);
        const auto stage = sdbz::parse_stg(data);
        std::array<std::size_t, 7> kind_counts{};
        std::size_t reference_count = 0;
        for (const auto& node : stage.m_nodes)
        {
            ++kind_counts[node.m_kind];
            reference_count += node.m_references.size();
        }
        std::cout << "STG scene\n"
                  << "Version:              0x" << std::hex << stage.m_version << std::dec << '\n'
                  << "Names:                " << stage.m_names.size() << '\n'
                  << "Template roots:       " << stage.m_template_root_count << '\n'
                  << "Root nodes:           " << stage.m_roots.size() << '\n'
                  << "Nodes:                " << stage.m_nodes.size() << '\n'
                  << "Inline references:    " << reference_count << '\n'
                  << "EFFECT root:          " << stage.m_effect_root << '\n'
                  << "STAGE root:           " << stage.m_stage_root << '\n'
                  << "Node kinds 1..6:      ";
        for (std::size_t kind = 1; kind < kind_counts.size(); ++kind)
        {
            std::cout << (kind == 1 ? "" : "/") << kind_counts[kind];
        }
        std::cout << '\n';
        for (const auto& node : stage.m_nodes)
        {
            std::cout << "  [" << std::setw(4) << node.m_index << "] kind=" << static_cast<unsigned int>(node.m_kind)
                      << " parent=" << node.m_parent << " links=0x" << std::hex
                      << static_cast<unsigned int>(node.m_links) << " flags=0x"
                      << static_cast<unsigned int>(node.m_flags) << std::dec
                      << " name=" << printable_name(node.m_name) << '\n';
        }
        return 0;
    }

    int command_hit(const std::filesystem::path& path)
    {
        const auto data = sdbz::read_file(path);
        const auto hit = sdbz::parse_hit(data);
        std::size_t polygons = 0;
        for (const auto& object : hit.m_objects)
        {
            polygons += object.m_polygon_count;
        }
        std::cout << "HIT collision resource\n"
                  << "Objects:              " << hit.m_objects.size() << '\n'
                  << "Polygons:             " << hit.m_polygons.size() << '\n'
                  << "Vertices:             " << hit.m_vertices.size() << '\n'
                  << "Object polygon refs:  " << polygons << '\n';
        for (const auto& object : hit.m_objects)
        {
            std::cout << "  " << printable_name(object.m_name)
                      << " flags=0x" << std::hex << object.m_flags << std::dec
                      << " polygons=" << object.m_polygon_count
                      << " first=" << object.m_first_polygon
                      << " radius=" << object.m_radius << '\n';
        }
        return 0;
    }

    int command_hit_obj(const std::filesystem::path& input_path, const std::filesystem::path& output_path)
    {
        const auto data = sdbz::read_file(input_path);
        const auto hit = sdbz::parse_hit(data);
        sdbz::write_hit_obj(output_path, hit);
        std::cout << "Exported " << hit.m_objects.size() << " HIT objects and " << hit.m_polygons.size()
                  << " polygons to " << output_path.string() << " (resource-local coordinates)\n";
        return 0;
    }

    int command_model_obj(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_path)
    {
        const auto data = sdbz::read_file(input_path);
        const auto model = sdbz::parse_fmdl(data, input_path.stem().string());
        sdbz::write_model_obj(output_path, model);
        std::cout << "Exported " << model.m_meshes.size() << " meshes from " << input_path.string()
                  << " to " << output_path.string() << " (static bind pose)\n";
        return 0;
    }

    int command_extract(
        const std::filesystem::path& info_path,
        const std::filesystem::path& game_path,
        const std::filesystem::path& output_path,
        const bool overwrite)
    {
        const auto encoded = sdbz::read_file(info_path);
        const auto game_size = std::filesystem::file_size(game_path);
        const auto archive = sdbz::s_archive::from_encoded_info(encoded, game_size);
        const auto count = archive.extract_all(game_path, output_path, overwrite);
        std::cout << "Extracted " << count << " assets to " << output_path.string() << '\n';
        return 0;
    }
}

int main(const int argc, char** argv)
{
    try
    {
        if (argc < 2)
        {
            print_usage();
            return 1;
        }

        const std::string command = argv[1];
        if (command == "elf" && argc == 3)
        {
            return command_elf(argv[2]);
        }
        if (command == "info" && (argc == 3 || argc == 4))
        {
            const std::filesystem::path game_path = argc == 4 ? argv[3] : "";
            return command_info(argv[2], argc == 4 ? &game_path : nullptr);
        }
        if (command == "container" && argc == 3)
        {
            return command_container(argv[2]);
        }
        if (command == "texture" && argc == 4)
        {
            return command_texture(argv[2], argv[3]);
        }
        if (command == "model" && argc == 3)
        {
            return command_model(argv[2]);
        }
        if (command == "model-obj" && argc == 4)
        {
            return command_model_obj(argv[2], argv[3]);
        }
        if (command == "motion" && argc == 3)
        {
            return command_motion(argv[2]);
        }
        if (command == "stage" && argc == 3)
        {
            return command_stage(argv[2]);
        }
        if (command == "hit" && argc == 3)
        {
            return command_hit(argv[2]);
        }
        if (command == "hit-obj" && argc == 4)
        {
            return command_hit_obj(argv[2], argv[3]);
        }
        if (command == "rebuild" && argc == 6)
        {
            return command_rebuild(argv[2], argv[3], argv[4], argv[5]);
        }
        if (command == "extract" && (argc == 5 || argc == 6))
        {
            const bool overwrite = argc == 6 && std::string(argv[5]) == "--overwrite";
            if (argc == 6 && !overwrite)
            {
                throw std::runtime_error("unknown extract option: " + std::string(argv[5]));
            }
            return command_extract(argv[2], argv[3], argv[4], overwrite);
        }

        print_usage();
        return 1;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "error: " << exception.what() << '\n';
        return 2;
    }
}
