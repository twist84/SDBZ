#include "core/fon.hpp"
#include "core/binary_reader.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace sdbz
{
    s_fon parse_fon(std::span<const std::uint8_t> data)
    {
        if (data.size() < 0x1EA0)
        {
            throw std::runtime_error("file too small for FON header");
        }

        if (data[0] != 'F' || data[1] != 'F' || data[2] != 'O' || data[3] != 'N')
        {
            throw std::runtime_error("bad FON magic");
        }

        s_fon result;
        result.m_unknown0 = read_u32_le(data, 0x04).value_or(0);
        result.m_version = read_u32_le(data, 0x08).value_or(0);
        result.m_unknown_c = read_u32_le(data, 0x0C).value_or(0);
        result.m_page_count = read_u32_le(data, 0x10).value_or(0);
        result.m_last_column = read_u32_le(data, 0x14).value_or(0);
        result.m_glyph_count = read_u32_le(data, 0x18).value_or(0);
        result.m_width = data[0x1C];
        result.m_height = data[0x1D];
        result.m_bits_per_pixel = data[0x1E];
        result.m_bytes_per_row = data[0x1F];
        result.m_unknown_20 = read_u32_le(data, 0x20).value_or(0);

        for (std::size_t i = 0; i < 96; ++i)
        {
            result.m_secondary_remap[i] = data[0x40 + i];
        }

        const std::size_t map_count = static_cast<std::size_t>(result.m_page_count) * (result.m_last_column + 1);
        const std::size_t map_offset = 0xA0;
        if (map_offset + map_count * 2 > data.size())
        {
            throw std::runtime_error("FON char map out of range");
        }

        result.m_char_map.resize(map_count);
        for (std::size_t i = 0; i < map_count; ++i)
        {
            result.m_char_map[i] = read_u16_le(data, map_offset + i * 2).value_or(0);
        }

        const std::size_t glyph_offset = 0x1EA0;
        const std::size_t bytes_per_glyph = static_cast<std::size_t>(result.m_bytes_per_row) * result.m_height;
        if (glyph_offset + result.m_glyph_count * bytes_per_glyph != data.size())
        {
            throw std::runtime_error("FON glyph data size mismatch");
        }

        result.m_glyphs.resize(result.m_glyph_count);
        for (std::uint32_t g = 0; g < result.m_glyph_count; ++g)
        {
            result.m_glyphs[g].assign(
                data.begin() + static_cast<std::ptrdiff_t>(glyph_offset + g * bytes_per_glyph),
                data.begin() + static_cast<std::ptrdiff_t>(glyph_offset + (g + 1) * bytes_per_glyph));
        }

        return result;
    }
}
