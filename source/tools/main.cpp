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
#include <print>
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
        std::print(
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
            "  sdbz_tool rebuild <INFO.DAT> <GAME.DAT> <replacement_dir|-> <output_dir>\n");
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

        std::print("File:                 {}\n"
                   "Size:                 {} bytes\n"
                   "Class:                ELF32\n"
                   "Data:                 little-endian\n"
                   "Type:                 {}\n"
                   "Machine:              {}\n"
                   "Entry point:          {:#x}\n"
                   "ELF flags:            {:#x}\n"
                   "Program headers:      {}\n"
                   "Section headers:      {}\n",
                   path.string(), data.size(),
                   (elf.m_type == 2 ? "executable" : std::to_string(elf.m_type)),
                   sdbz::elf_machine_name(elf.m_machine),
                   elf.m_entry, elf.m_flags,
                   elf.m_program_header_count, elf.m_section_header_count);

        if (elf.m_machine == 8 && (elf.m_flags & 0xF0000000u) == 0x20000000u)
        {
            std::println("PS2 hint:             MIPS-III / R5900 flag family");
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

        std::print("Entries:              {}\n"
                   "Indexed bytes:        {}\n"
                   "INFO.DAT size:        {}\n",
                   archive.m_entries.size(), archive.m_used_bytes, archive.m_encoded_info.size());

        if (game_path)
        {
            std::print("GAME.DAT size:        {}\n", game_size);
        }

        std::print("\n{:<7}{:<14}{:<14}Name\n", "Index", "Sector", "Size");
        for (const auto& entry : archive.m_entries)
        {
            std::print("{:<7}{:<14}{:<14}{}\n",
                       entry.m_index, entry.m_sector_offset, entry.m_byte_size,
                       printable_name(entry.m_name_cp932));
        }
        return 0;
    }

    int command_container(const std::filesystem::path& path)
    {
        const auto data = sdbz::read_file(path);
        const auto container = sdbz::parse_chunked_container(data);
        std::print("Container:            {}\n"
                   "File size:            {}\n"
                   "Chunk count:          {}\n"
                   "Format:               {:#x}\n",
                   std::string(container.m_magic.begin(), container.m_magic.end()),
                   container.m_file_size, container.m_chunk_count, container.m_format);

        for (std::size_t index = 0; index < container.m_chunks.size(); ++index)
        {
            const auto& chunk = container.m_chunks[index];
            std::print("  [{:0>4}] {} offset={:#x} size={:#x}\n",
                       index, chunk.magic_string(), chunk.m_offset, chunk.m_size);
        }
        return 0;
    }

    int command_texture(const std::filesystem::path& input_path, const std::filesystem::path& output_path)
    {
        const auto data = sdbz::read_file(input_path);
        const auto image = sdbz::decode_first_fpix_texture(data);
        sdbz::write_bmp_rgba(output_path, image);
        std::print("Decoded {}x{} texture to {}\n",
           image.width, image.height, output_path.string());
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

        std::print("Model:                {}\n"
           "File size:            {} bytes\n"
           "Hierarchy nodes:      {}\n"
           "Materials:            {}\n"
           "Meshes:               {}\n"
           "Vertices:             {}\n"
           "Triangles:            {}\n"
           "Skinned meshes:       {}\n"
           "Morph meshes:         {}\n",
           model.m_name, data.size(), model.m_nodes.size(), model.m_materials.size(),
           model.m_meshes.size(), total_vertices, total_triangles, skinned_meshes, morph_meshes);

        std::print("\nMeshes\n{:<5}{:<28}{:<12}{:<12}Flags\n", "#", "Name", "Vertices", "Triangles");
        for (std::size_t index = 0; index < model.m_meshes.size(); ++index)
        {
            const auto& mesh = model.m_meshes[index];
            std::size_t triangles = 0;
            for (const auto& primitive : mesh.m_primitives)
            {
                triangles += primitive.m_indices.size() / 3;
            }
            std::print("{:<5}{:<28}{:<12}{:<12}0x{:x}",
                       index, printable_name(mesh.m_name), mesh.m_vertices.size(), triangles, mesh.m_vertex_flags);
            if (mesh.m_is_skinned)
            {
                std::print(" skinned");
            }
            if (mesh.m_has_morph)
            {
                std::print(" morph");
            }
            std::println("");
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
        std::print("Rebuilt archive with {} indexed assets in {}\n",
           archive.m_entries.size(), output_path.string());
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
        std::print("  [{:>3}] {} frames={} fps={} nodes={} channels(T/R/S)={}/{}/{} animated-nodes={}/{}/{}\n",
           index, printable_name(clip.m_name), clip.m_frame_count, clip.m_frames_per_second,
           clip.m_nodes.size(), clip.m_channel_counts[0], clip.m_channel_counts[1], clip.m_channel_counts[2],
           animated_nodes[0], animated_nodes[1], animated_nodes[2]);
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
            std::print("MOT2: {}\nFPS:                  {}\nFrame count:          {}\n"
               "Duration:             {} seconds\n"
               "Last frame time:      {} seconds\n"
               "Nodes:                {}\n"
               "Channels T/R/S:       {}/{}/{}\n",
               printable_name(clip.m_name), clip.m_frames_per_second,
               clip.m_frame_count, clip.m_duration, clip.m_last_frame_time,
               clip.m_nodes.size(), clip.m_channel_counts[0], clip.m_channel_counts[1], clip.m_channel_counts[2]);
            return 0;
        }

        const auto package = sdbz::parse_rfsm(data);
        std::print("RFSM/L3 motion package\n"
                   "Bone names:           {}\n"
                   "Motions:              {}\n",
                   package.m_bone_names.size(), package.m_motions.size());
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
        std::print("STG scene\n"
           "Version:              {:#x}\n"
           "Names:                {}\n"
           "Template roots:       {}\n"
           "Root nodes:           {}\n"
           "Nodes:                {}\n"
           "Inline references:    {}\n"
           "EFFECT root:          {}\n"
           "STAGE root:           {}\n"
           "Node kinds 1..6:      ",
           stage.m_version, stage.m_names.size(), stage.m_template_root_count,
           stage.m_roots.size(), stage.m_nodes.size(), reference_count,
           stage.m_effect_root, stage.m_stage_root);

        for (std::size_t kind = 1; kind < kind_counts.size(); ++kind)
        {
            if (kind > 1) std::print("/");
            std::print("{}", kind_counts[kind]);
        }
        std::print("\n");

        for (const auto& node : stage.m_nodes)
        {
            std::print("  [{:>4}] kind={} parent={} links=0x{:x} flags=0x{:x} name={}\n",
                       node.m_index, static_cast<unsigned int>(node.m_kind),
                       node.m_parent, static_cast<unsigned int>(node.m_links),
                       static_cast<unsigned int>(node.m_flags), printable_name(node.m_name));
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
        std::print("HIT collision resource\n"
                   "Objects:              {}\n"
                   "Polygons:             {}\n"
                   "Vertices:             {}\n"
                   "Object polygon refs:  {}\n",
                   hit.m_objects.size(), hit.m_polygons.size(), hit.m_vertices.size(), polygons);
        for (const auto& object : hit.m_objects)
        {
            std::print("  {} flags=0x{:x} polygons={} first={} radius={}\n",
                       printable_name(object.m_name), object.m_flags,
                       object.m_polygon_count, object.m_first_polygon, object.m_radius);
        }
        return 0;
    }

    int command_hit_obj(const std::filesystem::path& input_path, const std::filesystem::path& output_path)
    {
        const auto data = sdbz::read_file(input_path);
        const auto hit = sdbz::parse_hit(data);
        sdbz::write_hit_obj(output_path, hit);
        std::print("Exported {} HIT objects and {} polygons to {} (resource-local coordinates)\n",
           hit.m_objects.size(), hit.m_polygons.size(), output_path.string());
        return 0;
    }

    int command_model_obj(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_path)
    {
        const auto data = sdbz::read_file(input_path);
        const auto model = sdbz::parse_fmdl(data, input_path.stem().string());
        sdbz::write_model_obj(output_path, model);
        std::print("Exported {} meshes from {} to {} (static bind pose)\n",
           model.m_meshes.size(), input_path.string(), output_path.string());
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
        std::print("Extracted {} assets to {}\n", count, output_path.string());
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
        std::println(std::cerr, "error: {}", exception.what());
        return 2;
    }
}
