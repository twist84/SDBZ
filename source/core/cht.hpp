#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    struct s_cht_variant
    {
        std::uint16_t m_name_index = 0;
        std::uint16_t m_resource_bank = 0;
        std::string m_motion_name;
    };

    struct s_cht_action
    {
        std::uint32_t m_motion_id = 0;
        std::vector<s_cht_variant> m_variants;
    };

    struct s_cht
    {
        std::vector<std::string> m_names;
        std::array<std::uint32_t, 18> m_flags{};
        std::vector<std::uint8_t> m_directory;
        std::vector<s_cht_action> m_actions;
    };

    [[nodiscard]] s_cht parse_cht(std::span<const std::uint8_t> data);
}
