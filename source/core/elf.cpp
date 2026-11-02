#include "core/elf.hpp"

#include "core/binary_reader.hpp"

#include <stdexcept>

namespace sdbz
{
    namespace
    {
        constexpr std::uint8_t k_elf_magic[] = { 0x7F, 'E', 'L', 'F' };
    }

    s_elf32_info inspect_elf32(const std::span<const std::uint8_t> data)
    {
        if (data.size() < 52)
        {
            throw std::runtime_error("ELF file is smaller than the ELF32 header");
        }
        for (std::size_t index = 0; index < 4; ++index)
        {
            if (data[index] != k_elf_magic[index])
            {
                throw std::runtime_error("file does not have the ELF signature");
            }
        }
        if (data[4] != 1)
        {
            throw std::runtime_error("ELF is not 32-bit");
        }
        if (data[5] != 1)
        {
            throw std::runtime_error("ELF is not little-endian");
        }
        if (data[6] != 1)
        {
            throw std::runtime_error("unsupported ELF identification version");
        }

        s_elf32_info result;
        result.m_class = data[4];
        result.m_data_encoding = data[5];
        result.m_type = *read_u16_le(data, 16);
        result.m_machine = *read_u16_le(data, 18);
        result.m_entry = *read_u32_le(data, 24);
        result.m_program_header_offset = *read_u32_le(data, 28);
        result.m_section_header_offset = *read_u32_le(data, 32);
        result.m_flags = *read_u32_le(data, 36);
        const auto header_size = read_u16_le(data, 40);
        const auto program_header_size = read_u16_le(data, 42);
        const auto program_header_count = read_u16_le(data, 44);
        const auto section_header_size = read_u16_le(data, 46);
        const auto section_header_count = read_u16_le(data, 48);
        if (!header_size || !program_header_size || !program_header_count || !section_header_size || !section_header_count)
        {
            throw std::runtime_error("ELF header is truncated");
        }
        if (*header_size != 52)
        {
            throw std::runtime_error("unexpected ELF32 header size");
        }

        result.m_program_header_size = *program_header_size;
        result.m_program_header_count = *program_header_count;
        result.m_section_header_size = *section_header_size;
        result.m_section_header_count = *section_header_count;

        const std::uint64_t program_end = static_cast<std::uint64_t>(result.m_program_header_offset) +
            static_cast<std::uint64_t>(result.m_program_header_size) * result.m_program_header_count;
        if (result.m_program_header_count != 0 &&
            (result.m_program_header_size < 32 || program_end > data.size()))
        {
            throw std::runtime_error("ELF program header table is outside the file");
        }

        const std::uint64_t section_end = static_cast<std::uint64_t>(result.m_section_header_offset) +
            static_cast<std::uint64_t>(result.m_section_header_size) * result.m_section_header_count;
        if (result.m_section_header_count != 0 &&
            (result.m_section_header_size < 40 || section_end > data.size()))
        {
            throw std::runtime_error("ELF section header table is outside the file");
        }

        return result;
    }

    std::string elf_machine_name(const std::uint16_t machine)
    {
        switch (machine)
        {
        case 8: return "MIPS";
        case 3: return "Intel 80386";
        case 40: return "ARM";
        case 62: return "AMD x86-64";
        case 183: return "AArch64";
        default: return "unknown (" + std::to_string(machine) + ")";
        }
    }
}
