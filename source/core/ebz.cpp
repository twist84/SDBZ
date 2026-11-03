#include "core/ebz.hpp"
#include "core/binary_reader.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    s_ebz parse_ebz(std::span<const std::uint8_t> data)
    {
        if (data.size() < 0x30)
        {
            throw std::runtime_error("file too small for EBZ header");
        }

        if (data[0] != ' ' || data[1] != 'Z' || data[2] != 'B' || data[3] != 'E')
        {
            throw std::runtime_error("bad EBZ magic");
        }

        s_ebz result;
        result.m_version = read_u16_le(data, 4).value();
        result.m_type = read_u16_le(data, 6).value();
        result.m_size_field = read_u32_le(data, 0x08).value();

        const auto param0_raw = read_u32_le(data, 0x0C).value();
        std::uint32_t bits = param0_raw;
        float f;
        std::memcpy(&f, &bits, 4);
        result.m_param0 = f;

        result.m_count = read_u32_le(data, 0x1C).value();
        result.m_u1 = read_u16_le(data, 0x20).value();
        result.m_u2 = read_u16_le(data, 0x22).value();
        result.m_u3 = read_u16_le(data, 0x24).value();
        result.m_p0 = read_u32_le(data, 0x26).value();
        result.m_p1 = read_u32_le(data, 0x2A).value();
        result.m_p2 = read_u32_le(data, 0x2E).value();

        std::size_t offset = result.m_size_field;
        if (offset + 8 > data.size())
        {
            throw std::runtime_error("invalid ref table offset");
        }
        std::uint32_t table_size = read_u32_le(data, offset).value();
        std::uint32_t table_count = read_u32_le(data, offset + 4).value();

        result.m_refs.reserve(table_count);
        for (std::uint32_t i = 0; i < table_count; ++i)
        {
            std::size_t roff = offset + 8 + i * 16;
            if (roff + 16 > data.size()) break;

            s_ebz_ref ref;
            ref.m_index = i;
            char name_buf[9] = {};
            std::memcpy(name_buf, data.data() + roff, 8);
            ref.m_name = name_buf;
            ref.m_value = static_cast<std::uint64_t>(read_u32_le(data, roff + 8).value()) |
                          (static_cast<std::uint64_t>(read_u32_le(data, roff + 12).value()) << 32);
            result.m_refs.push_back(ref);
        }

        std::size_t entries_offset = offset + table_size;
        result.m_entries.reserve(result.m_count);
        for (std::uint32_t i = 0; i < result.m_count; ++i)
        {
            std::size_t eoff = entries_offset + i * 112;
            if (eoff + 112 > data.size()) break;

            s_ebz_entry entry;
            entry.m_index = i;
            for (int f = 0; f < 28; ++f)
            {
                entry.m_fields[f] = read_u32_le(data, eoff + f * 4).value();
            }
            result.m_entries.push_back(entry);
        }

        result.m_remaining = data.size() - (entries_offset + result.m_count * 112);
        return result;
    }
}
