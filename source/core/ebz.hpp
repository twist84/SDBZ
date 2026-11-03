#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    struct s_ebz_ref
    {
        std::uint32_t m_index = 0;
        std::string m_name;
        std::uint64_t m_value = 0;
    };

    struct s_ebz_entry
    {
        std::uint32_t m_index = 0;
        std::array<std::uint32_t, 28> m_fields{};
    };

    struct s_ebz
    {
        std::uint16_t m_version = 0;
        std::uint16_t m_type = 0;
        std::uint32_t m_size_field = 0;
        float m_param0 = 0.0f;
        std::uint32_t m_count = 0;
        std::uint16_t m_u1 = 0, m_u2 = 0, m_u3 = 0;
        std::uint32_t m_p0 = 0, m_p1 = 0, m_p2 = 0;
        std::vector<s_ebz_ref> m_refs;
        std::vector<s_ebz_entry> m_entries;
        std::size_t m_remaining = 0;
    };

    s_ebz parse_ebz(std::span<const std::uint8_t> data);
}
