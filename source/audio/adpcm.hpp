#pragma once

#include <cstdint>
#include <vector>

namespace sdbz
{
    // PS ADPCM decoder (matches arika_audio.py decode_adpcm).
    // Decodes 16-byte frames (1 header + 15 sample bytes = 28 samples)
    // into 16-bit signed PCM samples.
    struct s_adpcm_decoder
    {
        static constexpr int k_coefficients[5][2] = {
            {0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}
        };

        std::int16_t m_history1 = 0;
        std::int16_t m_history2 = 0;

        std::vector<std::int16_t> decode(std::span<const std::uint8_t> data);
    };
}
