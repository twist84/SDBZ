#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace sdbz {

// PS ADPCM decoder (matching arika_audio.py decode_adpcm)
struct s_adpcm_decoder
{
    std::int16_t m_history1 = 0;
    std::int16_t m_history2 = 0;

    void reset()
    {
        m_history1 = 0;
        m_history2 = 0;
    }

    std::vector<std::int16_t> decode(std::span<const std::uint8_t> data);
};

} // namespace sdbz

