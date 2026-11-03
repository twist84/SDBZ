#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace sdbz
{
    struct s_fon
    {
        std::uint32_t m_unknown0 = 0;
        std::uint32_t m_version = 0;
        std::uint32_t m_unknown_c = 0;
        std::uint32_t m_page_count = 0;
        std::uint32_t m_last_column = 0;
        std::uint32_t m_glyph_count = 0;
        std::uint8_t m_width = 0;
        std::uint8_t m_height = 0;
        std::uint8_t m_bits_per_pixel = 0;
        std::uint8_t m_bytes_per_row = 0;
        std::uint32_t m_unknown_20 = 0;
        std::array<std::uint8_t, 96> m_secondary_remap{};
        std::vector<std::uint16_t> m_char_map;
        std::vector<std::vector<std::uint8_t>> m_glyphs;
    };

    [[nodiscard]] s_fon parse_fon(std::span<const std::uint8_t> data);
}
