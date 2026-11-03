#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    struct s_ase_sample
    {
        std::uint32_t m_pitch = 0;
        std::vector<std::uint8_t> m_payload;
    };

    struct s_ase
    {
        std::uint32_t m_version = 0;
        std::uint32_t m_sample_count = 0;
        std::uint32_t m_payload_byte_count = 0;
        std::vector<s_ase_sample> m_samples;
    };

    [[nodiscard]] s_ase parse_ase(std::span<const std::uint8_t> data);
}
