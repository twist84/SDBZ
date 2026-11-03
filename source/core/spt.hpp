#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    struct s_spt_token
    {
        std::uint32_t m_offset = 0;
        std::uint8_t m_type = 0;
        std::uint32_t m_value = 0;
        std::uint32_t m_byte_length = 0;
        std::uint32_t m_command_id = 0;
        std::uint32_t m_extra_length = 0;
        std::vector<std::uint8_t> m_extra;
        std::string m_text;
        std::uint16_t m_variable_index = 0;
        std::uint16_t m_variable_type = 0;
    };

    struct s_spt_program
    {
        std::uint32_t m_reserved = 0;
        std::uint32_t m_variable_count = 0;
        std::uint32_t m_bytecode_size = 0;
        std::array<std::uint32_t, 4> m_entry_points{};
        std::vector<s_spt_token> m_tokens;
        std::uint32_t m_size = 0;
        std::uint32_t m_file_offset = 0;
    };

    struct s_spt
    {
        std::uint32_t m_program_count = 0;
        std::vector<s_spt_program> m_programs;
        std::vector<std::uint8_t> m_trailing;
    };

    [[nodiscard]] s_spt parse_spt(std::span<const std::uint8_t> data);
}
