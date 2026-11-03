#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace sdbz
{
    struct s_ams
    {
        std::uint32_t m_version = 0;
        std::uint32_t m_pitch = 0;
        std::uint32_t m_valid_bytes_per_channel = 0;
        std::uint32_t m_unit_count = 0;
        std::vector<std::uint8_t> m_left_adpcm;
        std::vector<std::uint8_t> m_right_adpcm;
        std::vector<std::int16_t> m_left_samples;
        std::vector<std::int16_t> m_right_samples;
    };

    [[nodiscard]] s_ams parse_ams(std::span<const std::uint8_t> data);
}
