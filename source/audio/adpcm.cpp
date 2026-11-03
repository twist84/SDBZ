#include "core/binary_reader.hpp"
#include "audio/adpcm.hpp"

#include <algorithm>
#include <span>

namespace sdbz
{
    std::vector<std::int16_t> s_adpcm_decoder::decode(std::span<const std::uint8_t> data)
    {
        std::vector<std::int16_t> output;
        if (data.size() % 16 != 0)
        {
            throw std::runtime_error("partial ADPCM frame");
        }

        std::int16_t h1 = m_history1;
        std::int16_t h2 = m_history2;

        for (std::size_t at = 0; at < data.size(); at += 16)
        {
            const std::uint8_t header = data[at];
            const std::uint8_t flags = data[at + 1];
            const int predictor = header >> 4;
            const int shift = header & 15;

            if (predictor > 4 || shift > 12 || flags > 7)
            {
                throw std::runtime_error("invalid ADPCM frame");
            }

            const int c1 = k_coefficients[predictor][0];
            const int c2 = k_coefficients[predictor][1];

            for (std::size_t byte_offset = at + 2; byte_offset < at + 16; ++byte_offset)
            {
                const std::uint8_t byte = data[byte_offset];
                for (int nibble_index = 0; nibble_index < 2; ++nibble_index)
                {
                    const int nibble = (nibble_index == 0) ? (byte & 15) : (byte >> 4);
                    const int signed_nibble = (nibble & 8) ? (nibble - 16) : nibble;
                    int value = ((signed_nibble * 4096) >> shift) +
                        static_cast<int>((h1 * c1 + h2 * c2) / 64);
                    if (flags == 7)
                    {
                        value = 0;
                    }
                    value = std::clamp(value, -32768, 32767);
                    output.push_back(static_cast<std::int16_t>(value));
                    h2 = h1;
                    h1 = static_cast<std::int16_t>(value);
                }
            }
        }

        m_history1 = h1;
        m_history2 = h2;
        return output;
    }
}
