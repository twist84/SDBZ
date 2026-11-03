#include "core/ams.hpp"
#include "core/audio/adpcm.hpp"
#include "core/binary_reader.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace sdbz
{
    s_ams parse_ams(std::span<const std::uint8_t> data)
    {
        constexpr std::size_t k_header_size = 0x800;
        constexpr std::size_t k_block_size = 0x10000;
        constexpr std::size_t k_channel_size = 0x8000;
        constexpr std::size_t k_unit_size = 0x20000;

        if (data.size() < k_header_size)
        {
            throw std::runtime_error("file too small for AMS header");
        }

        s_ams result;
        result.m_version = read_u32_le(data, 0x00).value_or(0);
        result.m_pitch = read_u32_le(data, 0x04).value_or(0);
        result.m_valid_bytes_per_channel = read_u32_le(data, 0x08).value_or(0);
        result.m_unit_count = read_u32_le(data, 0x0C).value_or(0);

        if (result.m_version != 120)
        {
            throw std::runtime_error("bad AMS version");
        }

        const std::size_t expected_size = k_header_size + static_cast<std::size_t>(result.m_unit_count) * k_unit_size;
        if (data.size() < expected_size)
        {
            throw std::runtime_error("truncated AMS data");
        }

        const std::size_t block_count = static_cast<std::size_t>(result.m_unit_count) * 2;

        // Logical block order: even indices ascending, then odd indices descending.
        std::vector<std::size_t> logical_blocks;
        logical_blocks.reserve(block_count);
        for (std::size_t b = 0; b < block_count; b += 2)
        {
            logical_blocks.push_back(b);
        }
        if (block_count > 1)
        {
            for (std::size_t i = block_count; i-- > 0; )
            {
                if (i % 2 == 1)
                {
                    logical_blocks.push_back(i);
                }
            }
        }

        for (std::size_t block_index : logical_blocks)
        {
            const std::size_t unit_index = block_index / 2;
            const std::size_t in_unit = block_index % 2;
            const std::size_t block_offset = k_header_size + unit_index * k_unit_size + in_unit * k_block_size;

            result.m_left_adpcm.insert(result.m_left_adpcm.end(),
                data.begin() + static_cast<std::ptrdiff_t>(block_offset),
                data.begin() + static_cast<std::ptrdiff_t>(block_offset + k_channel_size));
            result.m_right_adpcm.insert(result.m_right_adpcm.end(),
                data.begin() + static_cast<std::ptrdiff_t>(block_offset + k_channel_size),
                data.begin() + static_cast<std::ptrdiff_t>(block_offset + k_block_size));
        }

        if (result.m_left_adpcm.size() < result.m_valid_bytes_per_channel)
        {
            throw std::runtime_error("AMS valid size exceeds available data");
        }
        result.m_left_adpcm.resize(result.m_valid_bytes_per_channel);
        result.m_right_adpcm.resize(result.m_valid_bytes_per_channel);

        s_adpcm_decoder left_decoder;
        s_adpcm_decoder right_decoder;
        result.m_left_samples = left_decoder.decode(result.m_left_adpcm);
        result.m_right_samples = right_decoder.decode(result.m_right_adpcm);

        return result;
    }
}
