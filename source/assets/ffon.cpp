#include "assets/assets.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>

bool load_ffon(const std::vector<std::uint8_t>& data, s_ffon_font& font)
{
    if (data.size() < 0x40 || std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "FFON")
    {
        std::cerr << "Not an FFON data blob\\n";
        return false;
    }

    const auto page_count  = sdbz::read_u32_le(std::span(data.data() + 0x10, 4), 0).value();
    const auto last_column = sdbz::read_u32_le(std::span(data.data() + 0x14, 4), 0).value();
    const auto glyph_count = sdbz::read_u32_le(std::span(data.data() + 0x18, 4), 0).value();
    const auto width  = data[0x1C];
    const auto height = data[0x1D];
    const auto row_stride = data[0x1F];

    const auto columns_per_page = last_column + 1;
    const auto char_map_count = page_count * columns_per_page;
    const auto glyph_data_offset = 0xA0 + char_map_count * 2;

    font.width = width;
    font.height = height;
    font.bytes_per_row = row_stride;
    font.glyph_count = glyph_count;
    font.bytes_per_glyph = row_stride * height;

    font.char_map.assign(char_map_count, 0);
    for (std::uint32_t slot = 0; slot < char_map_count; ++slot)
    {
        const auto glyph_id = sdbz::read_u16_le(std::span(data.data() + 0xA0 + slot * 2, 2), 0).value();
        font.char_map[slot] = glyph_id;
    }

    font.glyphs.resize(glyph_count);
    for (std::uint32_t gi = 0; gi < glyph_count; ++gi)
    {
        const auto offset = glyph_data_offset + gi * font.bytes_per_glyph;
        font.glyphs[gi].resize(font.bytes_per_glyph);
        std::memcpy(font.glyphs[gi].data(), data.data() + offset, font.bytes_per_glyph);
    }

    std::cout << "Loaded FFON font: " << glyph_count << " glyphs, "
        << (int)width << "x" << (int)height << '\n';
    return true;
}

std::pair<int, int> glyph_ink_bounds(const s_ffon_font& font, const std::vector<std::uint8_t>& data)
{
    int left = static_cast<int>(font.width);
    int right = 0;
    for (std::uint32_t y = 0; y < font.height; ++y)
    {
        for (std::uint32_t x = 0; x < font.width; ++x)
        {
            const auto packed = data[y * font.bytes_per_row + (x / 2)];
            const auto value = (x & 1) ? (packed >> 4) : (packed & 0x0F);
            if (value != 0)
            {
                left = std::min(left, static_cast<int>(x));
                right = std::max(right, static_cast<int>(x) + 1);
            }
        }
    }
    if (right == 0) return { 0, 0 };
    return { left, right };
}