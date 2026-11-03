#include "core/ase.hpp"
#include "core/binary_reader.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace sdbz
{
    s_ase parse_ase(std::span<const std::uint8_t> data)
    {
        constexpr std::size_t k_header_size = 0x800;
        if (data.size() < k_header_size)
        {
            throw std::runtime_error("file too small for ASE header");
        }

        s_ase result;
        result.m_version = read_u32_le(data, 0x00).value_or(0);
        result.m_sample_count = read_u32_le(data, 0x04).value_or(0);
        result.m_payload_byte_count = read_u32_le(data, 0x08).value_or(0);

        if (result.m_version != 100)
        {
            throw std::runtime_error("bad ASE version");
        }

        if (static_cast<std::size_t>(result.m_payload_byte_count) + k_header_size > data.size())
        {
            throw std::runtime_error("invalid ASE payload size");
        }

        const std::size_t payload_base = k_header_size;

        std::vector<std::uint32_t> pitches(result.m_sample_count);
        std::vector<std::uint32_t> offsets(result.m_sample_count);
        for (std::uint32_t i = 0; i < result.m_sample_count; ++i)
        {
            const std::size_t entry_offset = 0x0C + i * 8;
            if (entry_offset + 8 > k_header_size)
            {
                throw std::runtime_error("ASE sample table out of range");
            }
            pitches[i] = read_u32_le(data, entry_offset).value_or(0);
            offsets[i] = read_u32_le(data, entry_offset + 4).value_or(0);
        }

        result.m_samples.reserve(result.m_sample_count);
        for (std::uint32_t i = 0; i < result.m_sample_count; ++i)
        {
            s_ase_sample sample;
            sample.m_pitch = pitches[i];

            const std::uint32_t rel = offsets[i];
            std::uint32_t next_rel = result.m_payload_byte_count;
            if (i + 1 < result.m_sample_count)
            {
                next_rel = offsets[i + 1];
            }

            if (rel > result.m_payload_byte_count || next_rel < rel || next_rel > result.m_payload_byte_count)
            {
                throw std::runtime_error("invalid ASE sample bounds");
            }

            const std::size_t length = next_rel - rel;
            sample.m_payload.assign(
                data.begin() + static_cast<std::ptrdiff_t>(payload_base + rel),
                data.begin() + static_cast<std::ptrdiff_t>(payload_base + next_rel));

            if (sample.m_payload.size() != length)
            {
                throw std::runtime_error("ASE sample copy failed");
            }

            result.m_samples.push_back(std::move(sample));
        }

        return result;
    }
}
