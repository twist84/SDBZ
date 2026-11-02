#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace sdbz
{
    struct s_elf32_info
    {
        std::uint8_t m_class = 0;
        std::uint8_t m_data_encoding = 0;
        std::uint16_t m_type = 0;
        std::uint16_t m_machine = 0;
        std::uint32_t m_entry = 0;
        std::uint32_t m_program_header_offset = 0;
        std::uint32_t m_section_header_offset = 0;
        std::uint32_t m_flags = 0;
        std::uint16_t m_program_header_size = 0;
        std::uint16_t m_program_header_count = 0;
        std::uint16_t m_section_header_size = 0;
        std::uint16_t m_section_header_count = 0;
    };

    [[nodiscard]] s_elf32_info inspect_elf32(std::span<const std::uint8_t> data);
    [[nodiscard]] std::string elf_machine_name(std::uint16_t machine);
}
