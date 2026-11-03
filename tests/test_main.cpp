#include "core/animation.hpp"
#include "core/ase.hpp"
#include "core/ams.hpp"
#include "core/cht.hpp"
#include "core/fon.hpp"
#include "core/spt.hpp"
#include "core/binary_reader.hpp"
#include "core/container.hpp"
#include "core/ebz.hpp"
#include "core/elf.hpp"
#include "core/info_archive.hpp"
#include "core/model.hpp"
#include "core/stage.hpp"
#include "core/texture.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0;

    void require(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            ++g_failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void write_u16(std::vector<std::uint8_t>& data, const std::size_t offset, const std::uint16_t value)
    {
        data[offset + 0] = static_cast<std::uint8_t>(value);
        data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    }

    void write_u32(std::vector<std::uint8_t>& data, const std::size_t offset, const std::uint32_t value)
    {
        data[offset + 0] = static_cast<std::uint8_t>(value);
        data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
        data[offset + 2] = static_cast<std::uint8_t>(value >> 16);
        data[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    }

    void write_f32(std::vector<std::uint8_t>& data, const std::size_t offset, const float value)
    {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        write_u32(data, offset, bits);
    }

    std::vector<std::uint8_t> make_chunk(const char* magic, std::vector<std::uint8_t> data)
    {
        std::vector<std::uint8_t> result(32 + data.size(), 0);
        std::copy_n(magic, 4, result.begin());
        write_u32(result, 4, static_cast<std::uint32_t>(result.size()));
        std::copy(data.begin(), data.end(), result.begin() + 32);
        return result;
    }

    std::vector<std::uint8_t> make_directory(const char* magic,
        const std::vector<std::vector<std::uint8_t>>& chunks)
    {
        std::size_t size = 32 + chunks.size() * 4;
        for (const auto& chunk : chunks)
        {
            size += chunk.size();
        }
        std::vector<std::uint8_t> result(size, 0);
        std::copy_n(magic, 4, result.begin());
        write_u32(result, 4, static_cast<std::uint32_t>(size));
        write_u32(result, 8, static_cast<std::uint32_t>(chunks.size()));
        write_u32(result, 12, 0x100);
        std::size_t offset = 32 + chunks.size() * 4;
        for (std::size_t index = 0; index < chunks.size(); ++index)
        {
            write_u32(result, 32 + index * 4, static_cast<std::uint32_t>(offset));
            std::copy(chunks[index].begin(), chunks[index].end(), result.begin() + static_cast<std::ptrdiff_t>(offset));
            offset += chunks[index].size();
        }
        return result;
    }

    std::vector<std::uint8_t> make_test_xmod()
    {
        constexpr std::size_t dma_offset = 0x1A0;
        constexpr std::size_t dma_size = 16 + 12 * 16;
        std::vector<std::uint8_t> xmod(dma_offset + dma_size, 0);
        std::copy_n("XMOD", 4, xmod.begin());
        write_u32(xmod, 4, static_cast<std::uint32_t>(xmod.size()));
        write_u32(xmod, 12, 0);
        write_u32(xmod, 48, 3);
        write_u32(xmod, 52, 1);

        write_u32(xmod, dma_offset, 0x6000000Cu);
        write_u32(xmod, dma_offset + 4, 0);
        const std::size_t vif = dma_offset + 16;
        write_u32(xmod, vif, (0x6Cu << 24) | (11u << 16));
        const std::size_t packet = vif + 4;
        write_u32(xmod, packet + 16, 3);

        constexpr std::array<std::array<float, 3>, 3> positions =
        {{
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f }
        }};
        constexpr std::array<std::array<float, 2>, 3> uvs =
        {{
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 0.0f, 1.0f }
        }};

        for (std::size_t vertex = 0; vertex < 3; ++vertex)
        {
            const std::size_t offset = packet + 32 + vertex * 48;
            for (std::size_t component = 0; component < 3; ++component)
            {
                write_f32(xmod, offset + component * 4, positions[vertex][component]);
                write_f32(xmod, offset + 16 + component * 4, component == 2 ? 1.0f : 0.0f);
            }
            write_f32(xmod, offset + 28, 1.0f);
            if (vertex < 2)
            {
                write_u32(xmod, offset + 12, 0x8000u);
            }
            write_f32(xmod, offset + 32, uvs[vertex][0]);
            write_f32(xmod, offset + 36, uvs[vertex][1]);
        }
        return xmod;
    }

    std::vector<std::uint8_t> make_test_fmdl()
    {
        const auto xmod = make_test_xmod();
        const auto dmod = make_directory("DMOD", { xmod });

        std::vector<std::uint8_t> xmat_data(48, 0);
        for (std::size_t component = 0; component < 4; ++component)
        {
            write_f32(xmat_data, component * 4, 1.0f);
            write_f32(xmat_data, 16 + component * 4, 1.0f);
        }
        write_u32(xmat_data, 32, 0xFFFFFFFFu);
        auto xmat = make_chunk("XMAT", std::move(xmat_data));
        write_u32(xmat, 8, 1);

        std::vector<std::uint8_t> xmal_data(128, 0xFF);
        write_u32(xmal_data, 0, 0);
        for (std::size_t slot = 1; slot < 32; ++slot)
        {
            write_u32(xmal_data, slot * 4, 0xFFFFFFFFu);
        }
        const auto xmal = make_chunk("XMAL", std::move(xmal_data));
        const auto dmat = make_directory("DMAT", { xmat, xmal });

        std::vector<std::uint8_t> xmon_data(5, 0);
        std::copy_n("body", 4, xmon_data.begin());
        const auto xmon = make_chunk("XMON", std::move(xmon_data));

        std::vector<std::uint8_t> xhie_data(64, 0);
        write_u32(xhie_data, 0, 0);
        write_u32(xhie_data, 4, 0xFFFFFFFFu);
        write_u32(xhie_data, 8, 0xFFFFFFFFu);
        write_u32(xhie_data, 12, 0xFFFFFFFFu);
        for (std::size_t component = 0; component < 3; ++component)
        {
            write_f32(xhie_data, 16 + component * 4, 1.0f);
        }
        write_f32(xhie_data, 48, 5.0f);
        auto xhie = make_chunk("XHIE", std::move(xhie_data));
        write_u32(xhie, 8, 1);
        const auto dhie = make_directory("DHIE", { xhie });

        std::vector<std::uint8_t> xnon_data(5, 0);
        std::copy_n("root", 4, xnon_data.begin());
        const auto xnon = make_chunk("XNON", std::move(xnon_data));

        std::vector<std::uint8_t> xmtx_data(128, 0xFF);
        write_u32(xmtx_data, 0, 0);
        const auto xmtx = make_chunk("XMTX", std::move(xmtx_data));

        return make_directory("FMDL", { dmod, dmat, xmon, dhie, xnon, xmtx });
    }

    std::vector<std::uint8_t> make_test_mot2()
    {
        std::vector<std::uint8_t> data(160, 0);
        std::copy_n("MOT2", 4, data.begin());
        write_u32(data, 4, static_cast<std::uint32_t>(data.size()));
        write_f32(data, 8, 60.0f);
        write_u16(data, 12, 2);
        write_u16(data, 14, 1);
        write_u16(data, 16, 3);
        write_u16(data, 18, 1);
        write_u32(data, 20, 112);
        write_u32(data, 24, 64);

        write_u16(data, 28, 1);
        write_u16(data, 30, 2);
        write_u32(data, 32, 128);
        write_u32(data, 36, 120);

        write_f32(data, 64, 0.0f);
        write_f32(data, 68, 0.0f);
        write_f32(data, 72, 0.0f);
        write_f32(data, 76, 0.0f);
        write_f32(data, 80, 0.0f);
        write_f32(data, 84, 0.0f);
        write_f32(data, 88, 0.0f);
        write_f32(data, 92, 1.0f);
        write_f32(data, 96, 1.0f);
        write_f32(data, 100, 1.0f);
        write_f32(data, 104, 1.0f);

        write_u16(data, 112, 1);
        write_u16(data, 114, 0);
        write_u16(data, 116, 1);
        write_u16(data, 118, 2);
        write_f32(data, 120, 0.0f);
        write_f32(data, 124, 1.0f / 60.0f);
        write_f32(data, 128, 0.0f);
        write_f32(data, 132, 0.0f);
        write_f32(data, 136, 0.0f);
        write_f32(data, 140, 0.0f);
        write_f32(data, 144, 10.0f);
        write_f32(data, 148, 0.0f);
        write_f32(data, 152, 0.0f);
        write_f32(data, 156, 0.0f);
        return data;
    }

    std::vector<std::uint8_t> make_test_rfsm()
    {
        const auto mot2 = make_test_mot2();
        std::vector<std::uint8_t> data(260, 0);
        std::copy_n("RFSM", 4, data.begin());
        data[4] = 'L';
        data[5] = '3';
        write_u16(data, 6, 1);
        write_u32(data, 8, 48);
        write_u32(data, 12, 93);

        std::copy_n("idle", 4, data.begin() + 16);
        write_u32(data, 32, 96);
        write_u32(data, 36, static_cast<std::uint32_t>(mot2.size()));
        write_u32(data, 40, 0);
        write_u32(data, 44, 0);

        write_u16(data, 48, 1);
        write_u16(data, 50, 1);
        write_u32(data, 52, 64);
        write_u32(data, 56, 80);
        write_u32(data, 60, 92);
        std::copy_n("root", 4, data.begin() + 64);
        write_u32(data, 80, 0);
        write_u32(data, 84, 256);
        write_u32(data, 88, 258);
        std::copy(mot2.begin(), mot2.end(), data.begin() + 96);
        write_u16(data, 256, 0);
        write_u16(data, 258, 0);
        return data;
    }

    void test_animation_formats()
    {
        const auto mot2_data = make_test_mot2();
        const auto motion = sdbz::parse_mot2(mot2_data, "move");
        require(motion.m_name == "move", "MOT2 clip name");
        require(motion.m_nodes.size() == 1, "MOT2 node count");
        require(motion.m_channel_counts[0] == 1, "MOT2 translation channel count");
        require(motion.m_nodes[0].m_translation.m_is_animated, "MOT2 animated translation track");
        require(!motion.m_nodes[0].m_rotation.m_is_animated, "MOT2 constant rotation track");
        const auto sampled = sdbz::sample_animation_track(motion.m_nodes[0].m_translation, 1.0f / 120.0f);
        require(std::abs(sampled[0] - 5.0f) < 0.001f, "MOT2 linear translation sampling");

        sdbz::s_animation_track rotation;
        rotation.m_times = { 0.0f, 1.0f };
        rotation.m_values =
        {
            { 0.0f, 0.0f, 0.0f, 1.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f }
        };
        const auto halfway_rotation = sdbz::sample_animation_track(rotation, 0.5f, true);
        require(std::abs(halfway_rotation[2] - 0.7071067f) < 0.001f &&
            std::abs(halfway_rotation[3] - 0.7071067f) < 0.001f,
            "MOT2 shortest-path quaternion slerp");

        const auto package = sdbz::parse_rfsm(make_test_rfsm());
        require(package.m_bone_names.size() == 1 && package.m_bone_names[0] == "root", "RFSM shared bone names");
        require(package.m_motions.size() == 1, "RFSM motion count");
        require(package.m_motions[0].m_name == "idle", "RFSM motion name");
        require(package.m_motions[0].m_nodes[0].m_name == "root", "RFSM reverse bone binding");
    }

    std::vector<std::uint8_t> make_decoded_info()
    {
        std::vector<std::uint8_t> data(0x30 + 0x30, 0);
        const std::array<std::uint8_t, 16> signature =
        {
            0xC2, 'w', 'w', 'w', '.', 'a', 'r', 'i', 'k', 'a', '.', 'c', 'o', '.', 'j', 'p'
        };
        std::copy(signature.begin(), signature.end(), data.begin());
        write_u32(data, 0x2C, 1);
        const std::string name = "test/sample.pix";
        std::copy(name.begin(), name.end(), data.begin() + 0x30);
        write_u32(data, 0x30 + 32, 0);
        write_u32(data, 0x30 + 36, 0);
        write_u32(data, 0x30 + 40, 1);
        write_u32(data, 0x30 + 44, 5);
        return data;
    }

    std::vector<std::uint8_t> make_fpix()
    {
        constexpr std::size_t chunk_offset = 36;
        constexpr std::size_t chunk_size = 40;
        std::vector<std::uint8_t> data(chunk_offset + chunk_size, 0);
        std::copy_n("FPIX", 4, data.begin());
        write_u32(data, 4, static_cast<std::uint32_t>(data.size()));
        write_u32(data, 8, 1);
        write_u32(data, 12, 0x100);
        write_u32(data, 32, static_cast<std::uint32_t>(chunk_offset));
        std::copy_n("XPIX", 4, data.begin() + static_cast<std::ptrdiff_t>(chunk_offset));
        write_u32(data, chunk_offset + 4, static_cast<std::uint32_t>(chunk_size));
        std::copy_n("TIM2", 4, data.begin() + static_cast<std::ptrdiff_t>(chunk_offset + 32));
        return data;
    }

    void test_info_codec()
    {
        const auto decoded = make_decoded_info();
        const auto encoded = sdbz::encode_info(decoded);
        const auto roundtrip = sdbz::decode_info(encoded);
        require(roundtrip == decoded, "INFO.DAT codec roundtrip");

        auto all_values = decoded;
        all_values.resize(0x30 + 256 * 8);
        for (std::size_t index = 0x30; index < all_values.size(); ++index)
        {
            all_values[index] = static_cast<std::uint8_t>((index - 0x30) & 0xFF);
        }
        require(sdbz::decode_info(sdbz::encode_info(all_values)) == all_values,
            "INFO.DAT codec preserves every byte value");
    }

    void test_info_table()
    {
        const auto decoded = make_decoded_info();
        const auto archive = sdbz::s_archive::from_decoded_info(decoded, 2048);
        require(archive.m_entries.size() == 1, "synthetic INFO.DAT entry count");
        require(archive.m_entries[0].m_name_cp932 == "test/sample.pix", "synthetic INFO.DAT filename");
        require(archive.m_entries[0].offset() == 0, "synthetic INFO.DAT sector offset");
        require(archive.m_used_bytes == 2048, "synthetic INFO.DAT indexed size");

        bool rejected = false;
        try
        {
            (void)sdbz::s_archive::from_decoded_info(decoded, 2047);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        require(rejected, "INFO.DAT rejects GAME.DAT that is too small");
    }

    void test_archive_rebuild()
    {
        const auto root = std::filesystem::temp_directory_path() / "sdbz_archive_rebuild_test";
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root / "replacements" / "test", error);
        require(!error, "create archive rebuild fixture directory");

        const auto info_path = root / "source_INFO.DAT";
        const auto game_path = root / "source_GAME.DAT";
        const auto replacement_path = root / "replacements" / "test" / "sample.pix";
        const auto output_path = root / "rebuilt";

        const auto encoded_info = sdbz::encode_info(make_decoded_info());
        sdbz::write_file(info_path, encoded_info);

        std::vector<std::uint8_t> game_data(4096, 0x5A);
        std::copy_n("hello", 5, game_data.begin());
        sdbz::write_file(game_path, game_data);
        const std::string replacement = "replacement-data";
        sdbz::write_file(replacement_path,
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(replacement.data()), replacement.size()));

        const auto archive = sdbz::s_archive::from_encoded_info(encoded_info, game_data.size());
        archive.rebuild(info_path, game_path, root / "replacements", output_path);

        const auto rebuilt_info = sdbz::read_file(output_path / "INFO.DAT");
        const auto rebuilt_archive = sdbz::s_archive::from_encoded_info(rebuilt_info,
            std::filesystem::file_size(output_path / "GAME.DAT"));
        require(rebuilt_archive.m_entries.size() == 1, "rebuilt INFO.DAT retains asset count");
        require(rebuilt_archive.m_entries[0].m_byte_size == replacement.size(), "rebuilt INFO.DAT updates asset byte size");
        require(rebuilt_archive.m_entries[0].m_sector_offset == 0, "rebuilt INFO.DAT compacts first asset to sector zero");

        const auto rebuilt_game = sdbz::read_file(output_path / "GAME.DAT");
        require(rebuilt_game.size() == game_data.size(), "rebuilt GAME.DAT retains original capacity");
        require(std::equal(replacement.begin(), replacement.end(), rebuilt_game.begin()),
            "rebuilt GAME.DAT contains replacement bytes");
        require(rebuilt_game[replacement.size()] == 0, "rebuilt GAME.DAT zero-fills changed asset padding");
        require(rebuilt_game[2048] == 0x5A, "rebuilt GAME.DAT preserves the original reserve tail");

        std::filesystem::remove_all(root, error);
    }

    void test_reference_info()
    {
        const auto path = std::filesystem::path(SDBZ_TEST_DATA_DIR) / "INFO.decoded.eu_beta.bin";
        const auto decoded = sdbz::read_file(path);
        const auto archive = sdbz::s_archive::from_decoded_info(decoded);
        require(decoded.size() == 98304, "reference decoded INFO.DAT size");
        require(archive.m_entries.size() == 1934, "reference INFO.DAT has 1,934 entries");
        require(archive.m_entries.front().m_name_cp932 == "dis/.#main_menu.pix.1.9", "reference first resource name");
    }

    void test_container()
    {
        const auto data = make_fpix();
        const auto container = sdbz::parse_chunked_container(data, "FPIX");
        require(container.m_chunks.size() == 1, "FPIX chunk count");
        require(container.m_chunks[0].magic_string() == "XPIX", "FPIX child magic");
        require(container.m_chunks[0].m_offset == 36, "FPIX child offset");

        auto invalid = data;
        invalid[36 + 4] = 1;
        bool rejected = false;
        try
        {
            (void)sdbz::parse_chunked_container(invalid);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        require(rejected, "container parser rejects inconsistent chunk sizes");
    }

    std::vector<std::uint8_t> make_textured_fpix()
    {
        constexpr std::size_t chunk_offset = 36;
        constexpr std::size_t xpix_wrapper_size = 32;
        constexpr std::size_t tim2_size = 128 + 52;
        constexpr std::size_t chunk_size = xpix_wrapper_size + tim2_size;
        std::vector<std::uint8_t> data(chunk_offset + chunk_size, 0);

        std::copy_n("FPIX", 4, data.begin());
        write_u32(data, 4, static_cast<std::uint32_t>(data.size()));
        write_u32(data, 8, 1);
        write_u32(data, 12, 0x100);
        write_u32(data, 32, static_cast<std::uint32_t>(chunk_offset));

        std::copy_n("XPIX", 4, data.begin() + static_cast<std::ptrdiff_t>(chunk_offset));
        write_u32(data, chunk_offset + 4, static_cast<std::uint32_t>(chunk_size));

        const std::size_t tim2 = chunk_offset + xpix_wrapper_size;
        std::copy_n("TIM2", 4, data.begin() + static_cast<std::ptrdiff_t>(tim2));
        data[tim2 + 4] = 4;
        data[tim2 + 5] = 1;
        write_u16(data, tim2 + 6, 1);

        const std::size_t picture = tim2 + 128;
        write_u32(data, picture + 0, 52);
        write_u32(data, picture + 4, 0);
        write_u32(data, picture + 8, 4);
        write_u16(data, picture + 12, 48);
        write_u16(data, picture + 14, 0);
        data[picture + 17] = 1;
        data[picture + 18] = 0;
        data[picture + 19] = 3;
        write_u16(data, picture + 20, 1);
        write_u16(data, picture + 22, 1);
        data[picture + 48] = 255;
        data[picture + 49] = 0;
        data[picture + 50] = 0;
        data[picture + 51] = 128;
        return data;
    }

    void test_tim2_texture()
    {
        const auto data = make_textured_fpix();
        const auto image = sdbz::decode_first_fpix_texture(data);
        require(image.width == 1 && image.height == 1, "TIM2 dimensions");
        require(image.m_format == sdbz::e_texture_format::rgba32, "TIM2 RGBA32 format");
        require(image.m_rgba == std::vector<std::uint8_t>({ 255, 0, 0, 255 }), "TIM2 RGBA and alpha expansion");
    }

    std::vector<std::uint8_t> make_indexed_tim2(const std::uint8_t image_type)
    {
        const std::uint16_t width = 2;
        const std::uint16_t height = 1;
        const std::uint16_t colour_count = image_type == 4 ? 16 : 2;
        const std::uint32_t image_size = image_type == 4 ? 1 : 2;
        const std::uint32_t palette_size = static_cast<std::uint32_t>(colour_count) * 4;
        const std::uint32_t picture_size = 48 + image_size + palette_size;
        std::vector<std::uint8_t> data(16 + picture_size, 0);
        std::copy_n("TIM2", 4, data.begin());
        data[5] = 0;
        write_u16(data, 6, 1);
        const std::size_t picture = 16;
        write_u32(data, picture, picture_size);
        write_u32(data, picture + 4, palette_size);
        write_u32(data, picture + 8, image_size);
        write_u16(data, picture + 12, 48);
        write_u16(data, picture + 14, colour_count);
        data[picture + 17] = 1;
        data[picture + 18] = 0x83;
        data[picture + 19] = image_type;
        write_u16(data, picture + 20, width);
        write_u16(data, picture + 22, height);

        const std::size_t image = picture + 48;
        if (image_type == 4)
        {
            data[image] = 0x10;
        }
        else
        {
            data[image] = 1;
            data[image + 1] = 0;
        }
        const std::size_t palette = image + image_size;
        data[palette + 0] = 255;
        data[palette + 1] = 0;
        data[palette + 2] = 0;
        data[palette + 3] = 128;
        data[palette + 4] = 0;
        data[palette + 5] = 255;
        data[palette + 6] = 0;
        data[palette + 7] = 64;
        return data;
    }

    void test_indexed_tim2()
    {
        const auto indexed4 = sdbz::decode_tim2(make_indexed_tim2(4));
        require(indexed4.m_format == sdbz::e_texture_format::indexed4, "TIM2 indexed4 format");
        require(indexed4.m_rgba == std::vector<std::uint8_t>({ 255, 0, 0, 255, 0, 255, 0, 128 }),
            "TIM2 indexed4 palette lookup and alpha expansion");

        const auto indexed8 = sdbz::decode_tim2(make_indexed_tim2(5));
        require(indexed8.m_format == sdbz::e_texture_format::indexed8, "TIM2 indexed8 format");
        require(indexed8.m_rgba == std::vector<std::uint8_t>({ 0, 255, 0, 128, 255, 0, 0, 255 }),
            "TIM2 indexed8 palette lookup");
    }

    void test_fmdl_geometry()
    {
        const auto data = make_test_fmdl();
        const auto model = sdbz::parse_fmdl(data, "fixture");
        require(model.m_nodes.size() == 1, "FMDL hierarchy node count");
        require(model.m_nodes[0].m_name == "root", "FMDL hierarchy node name");
        require(model.m_materials.size() == 1, "FMDL material count");
        require(model.m_meshes.size() == 1, "FMDL mesh count");
        require(model.m_meshes[0].m_name == "body", "FMDL mesh name");
        require(model.m_meshes[0].m_rigid_node_index == 0, "FMDL rigid mesh hierarchy attachment");
        require(model.m_meshes[0].m_vertices.size() == 3, "FMDL vertex decoding");
        require(model.m_meshes[0].m_primitives.size() == 1, "FMDL primitive count");
        require(model.m_meshes[0].m_primitives[0].m_indices == std::vector<std::uint32_t>({ 0, 1, 2 }),
            "FMDL triangle-strip winding and indices");

        const auto output_path = std::filesystem::temp_directory_path() / "sdbz_test_model.obj";
        sdbz::write_model_obj(output_path, model);
        std::ifstream obj_file(output_path, std::ios::binary);
        const std::string obj_text((std::istreambuf_iterator<char>(obj_file)), std::istreambuf_iterator<char>());
        require(obj_text.find("v 5 0 0") != std::string::npos, "FMDL OBJ rigid hierarchy transform");
        require(obj_text.find("f 1/1/1 2/2/2 3/3/3") != std::string::npos, "FMDL OBJ face export");
        std::error_code remove_error;
        std::filesystem::remove(output_path, remove_error);
    }


    std::vector<std::uint8_t> make_test_stg()
    {
        std::vector<std::uint8_t> data(16 + 3 * 16 + 3 * 44, 0);
        write_u32(data, 0, 0x100);
        std::copy_n("STG ", 4, data.begin() + 4);
        write_u32(data, 8, 3);
        write_u32(data, 12, 0);
        const std::array<std::string, 3> names = { "EFFECT", "STAGE", "mesh" };
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            std::copy(names[index].begin(), names[index].end(), data.begin() + static_cast<std::ptrdiff_t>(16 + index * 16));
        }

        const std::size_t node_offset = 64;
        // EFFECT root.
        data[node_offset] = 1;
        write_u32(data, node_offset + 4, 0);
        write_f32(data, node_offset + 32, 1.0f);
        write_f32(data, node_offset + 36, 1.0f);
        write_f32(data, node_offset + 40, 1.0f);
        // STAGE root with a child.
        data[node_offset + 44] = 1;
        data[node_offset + 44 + 2] = 1;
        write_u32(data, node_offset + 48, 1);
        write_f32(data, node_offset + 44 + 32, 1.0f);
        write_f32(data, node_offset + 44 + 36, 1.0f);
        write_f32(data, node_offset + 44 + 40, 1.0f);
        // Child mesh node.
        data[node_offset + 88] = 2;
        write_u32(data, node_offset + 92, 2);
        write_f32(data, node_offset + 88 + 32, 1.0f);
        write_f32(data, node_offset + 88 + 36, 1.0f);
        write_f32(data, node_offset + 88 + 40, 1.0f);
        return data;
    }

    std::vector<std::uint8_t> make_test_hit()
    {
        constexpr std::size_t object_count = 1;
        constexpr std::size_t names_offset = 32;
        constexpr std::size_t objects_offset = names_offset + 16;
        constexpr std::size_t polygons_offset = objects_offset + 64;
        constexpr std::size_t vertices_offset = polygons_offset + 32;
        std::vector<std::uint8_t> data(vertices_offset + 3 * 16, 0);
        write_u32(data, 0, 0x20000);
        std::copy_n("HIT ", 4, data.begin() + 4);
        write_u32(data, 8, static_cast<std::uint32_t>(object_count));
        write_u32(data, 12, static_cast<std::uint32_t>(objects_offset));
        write_u32(data, 16, static_cast<std::uint32_t>(polygons_offset));
        write_u32(data, 20, static_cast<std::uint32_t>(vertices_offset));
        std::copy_n("floor", 5, data.begin() + names_offset);
        write_u32(data, objects_offset + 4, 1);
        write_u32(data, objects_offset + 8, 0);
        write_f32(data, objects_offset + 12, 10.0f);
        write_u32(data, polygons_offset, 3);
        write_u32(data, polygons_offset + 4, 0);
        write_f32(data, polygons_offset + 16 + 1 * 4, 1.0f);
        constexpr std::array<std::array<float, 3>, 3> vertices = {{
            { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }
        }};
        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            for (std::size_t component = 0; component < 3; ++component)
            {
                write_f32(data, vertices_offset + index * 16 + component * 4, vertices[index][component]);
            }
        }
        return data;
    }

    void test_stage_and_hit()
    {
        const auto stage_data = make_test_stg();
        const auto stage = sdbz::parse_stg(stage_data);
        require(stage.m_nodes.size() == 3, "STG node count");
        require(stage.m_roots == std::vector<std::uint32_t>({ 0, 1 }), "STG authored roots");
        require(stage.m_nodes[2].m_parent == 1, "STG child/sibling hierarchy");
        require(stage.m_effect_root == 0 && stage.m_stage_root == 1, "STG EFFECT/STAGE root indices");

        const auto hit_data = make_test_hit();
        const auto hit = sdbz::parse_hit(hit_data);
        require(hit.m_objects.size() == 1 && hit.m_objects[0].m_name == "floor", "HIT named object");
        require(hit.m_polygons.size() == 1 && hit.m_polygons[0].m_vertex_count == 3, "HIT polygon decoding");
        require(hit.m_vertices.size() == 3, "HIT vertex decoding");
        const auto output_path = std::filesystem::temp_directory_path() / "sdbz_test_hit.obj";
        sdbz::write_hit_obj(output_path, hit);
        std::ifstream obj_file(output_path, std::ios::binary);
        const std::string obj_text((std::istreambuf_iterator<char>(obj_file)), std::istreambuf_iterator<char>());
        require(obj_text.find("o floor") != std::string::npos, "HIT OBJ object name");
        require(obj_text.find("f 1 2 3") != std::string::npos, "HIT OBJ polygon export");
        std::error_code remove_error;
        std::filesystem::remove(output_path, remove_error);
    }

    void test_synthetic_elf()
    {
        std::vector<std::uint8_t> data(52 + 32, 0);
        data[0] = 0x7F;
        data[1] = 'E';
        data[2] = 'L';
        data[3] = 'F';
        data[4] = 1;
        data[5] = 1;
        data[6] = 1;
        data[16] = 2;
        data[18] = 8;
        write_u32(data, 24, 0x00100008);
        write_u32(data, 28, 52);
        write_u32(data, 36, 0x20924000);
        data[40] = 52;
        data[42] = 32;
        data[44] = 1;

        const auto info = sdbz::inspect_elf32(data);
        require(info.m_machine == 8, "ELF MIPS machine");
        require(info.m_entry == 0x00100008, "ELF entry point");
    }
}

int main()
{
    std::cout << "info_codec" << std::endl; test_info_codec();
    std::cout << "test_info_table();" << std::endl; test_info_table();
    std::cout << "archive" << std::endl; test_archive_rebuild();
    // EBZ test
    {
        const auto data = sdbz::read_file("assets/extracted/eff/e00/e00003.ebz");
        const auto ebz = sdbz::parse_ebz(data);
        require(ebz.m_version == 1, "EBZ version");
        require(ebz.m_count > 0, "EBZ has entries");
        require(!ebz.m_refs.empty(), "EBZ has refs");
        std::cout << "EBZ: version=" << ebz.m_version << " type=" << ebz.m_type
            << " count=" << ebz.m_count << " refs=" << ebz.m_refs.size()
            << " entries=" << ebz.m_entries.size() << '\n';
    }
    // ASE test
    {
        const auto data = sdbz::read_file("assets/extracted/ply/p01/p01.ase");
        const auto ase = sdbz::parse_ase(data);
        require(ase.m_version == 100, "ASE version");
        require(!ase.m_samples.empty(), "ASE has samples");
        std::cout << "ASE: version=" << ase.m_version
            << " samples=" << ase.m_samples.size() << '\n';
    }
    // AMS test
    {
        const auto data = sdbz::read_file("assets/extracted/sqm/menu_lp.ams");
        const auto ams = sdbz::parse_ams(data);
        require(ams.m_version == 120, "AMS version");
        require(!ams.m_left_samples.empty(), "AMS has left samples");
        require(!ams.m_right_samples.empty(), "AMS has right samples");
        std::cout << "AMS: version=" << ams.m_version
            << " pitch=" << ams.m_pitch
            << " units=" << ams.m_unit_count
            << " samples=" << ams.m_left_samples.size() << '\n';
    }
    // FON test
    {
        const auto data = sdbz::read_file("assets/extracted/font/5-24-2.fon");
        const auto fon = sdbz::parse_fon(data);
        require(fon.m_glyph_count == 1154, "FON glyph count");
        require(fon.m_char_map.size() == 3840, "FON char map");
        std::cout << "FON: glyphs=" << fon.m_glyph_count
            << " map=" << fon.m_char_map.size() << '\n';
    }
    // CHT test
    {
        try
        {
            const auto data = sdbz::read_file("assets/extracted/ply/p01/p01act.cht");
            const auto cht = sdbz::parse_cht(data);
            require(!cht.m_names.empty(), "CHT names");
            require(!cht.m_actions.empty(), "CHT actions");
            std::cout << "CHT: names=" << cht.m_names.size()
                << " actions=" << cht.m_actions.size() << '\n';
        }
        catch (const std::exception& e)
        {
            std::cerr << "CHT exception: " << e.what() << '\n';
            ++g_failures;
        }
    }
    // SPT test
    {
        const auto data = sdbz::read_file("assets/extracted/spt/stage_test.spt");
        const auto spt = sdbz::parse_spt(data);
        require(!spt.m_programs.empty(), "SPT programs");
        std::cout << "SPT: programs=" << spt.m_programs.size() << '\n';
    }
    //ref
    //cont
    //anim
    //test_tim2_texture();
    //test_indexed_tim2();
    //fmdl
    //stage
    //elf

    if (g_failures != 0)
    {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All native core tests passed\n";
    return 0;
}





