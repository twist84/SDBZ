#include "core/spt.hpp"
#include "core/binary_reader.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace sdbz
{
    s_spt parse_spt(std::span<const std::uint8_t> data)
    {
        if (data.size() < 4)
        {
            throw std::runtime_error("file too small for SPT bundle");
        }

        s_spt result;
        result.m_program_count = read_u32_le(data, 0).value_or(0);
        if (result.m_program_count == 0 || result.m_program_count > 1024)
        {
            throw std::runtime_error("invalid SPT program count");
        }
        if (4 + result.m_program_count * 4 > data.size())
        {
            throw std::runtime_error("truncated SPT directory");
        }

        std::vector<std::uint32_t> sizes(result.m_program_count);
        for (std::uint32_t i = 0; i < result.m_program_count; ++i)
        {
            sizes[i] = read_u32_le(data, 4 + i * 4).value_or(0);
        }

        std::size_t at = 4 + result.m_program_count * 4;
        result.m_programs.reserve(result.m_program_count);

        for (std::uint32_t index = 0; index < result.m_program_count; ++index)
        {
            const std::uint32_t size = sizes[index];
            if (size < 28 || at + size > data.size())
            {
                throw std::runtime_error("invalid SPT program extent");
            }

            s_spt_program program;
            program.m_file_offset = static_cast<std::uint32_t>(at);
            program.m_size = size;
            program.m_reserved = read_u32_le(data, at).value_or(0);
            program.m_variable_count = read_u32_le(data, at + 4).value_or(0);
            program.m_bytecode_size = read_u32_le(data, at + 8).value_or(0);
            for (std::size_t e = 0; e < 4; ++e)
            {
                program.m_entry_points[e] = read_u32_le(data, at + 12 + e * 4).value_or(0);
            }

            if (program.m_bytecode_size != size - 28)
            {
                throw std::runtime_error("SPT bytecode size mismatch");
            }

            const std::size_t start = at + 28;
            const std::size_t end = at + size;
            std::size_t cursor = start;

            while (cursor < end)
            {
                if (cursor + 5 > end)
                {
                    throw std::runtime_error("truncated SPT token");
                }
                const std::size_t token_start = cursor;
                const std::uint8_t kind = data[cursor];
                const std::uint32_t value = read_u32_le(data, cursor + 1).value_or(0);
                cursor += 5;

                s_spt_token token;
                token.m_offset = static_cast<std::uint32_t>(token_start - start);
                token.m_type = kind;
                token.m_value = value;

                if (kind == 0)
                {
                    // float bits
                }
                else if (kind == 1)
                {
                    if (cursor + value + 1 > end)
                    {
                        throw std::runtime_error("truncated SPT string");
                    }
                    token.m_text.assign(reinterpret_cast<const char*>(data.data() + cursor), value);
                    cursor += value + 1;
                }
                else if (kind == 2)
                {
                    const std::uint32_t extra_length = value >> 16;
                    const std::uint32_t command_id = value & 0xFFFF;
                    if (cursor + extra_length > end)
                    {
                        throw std::runtime_error("truncated SPT command data");
                    }
                    token.m_command_id = command_id;
                    token.m_extra_length = extra_length;
                    token.m_extra.assign(data.begin() + static_cast<std::ptrdiff_t>(cursor),
                        data.begin() + static_cast<std::ptrdiff_t>(cursor + extra_length));
                    cursor += extra_length;
                }
                else if (kind == 3)
                {
                    token.m_variable_index = static_cast<std::uint16_t>(value & 0xFFFF);
                    token.m_variable_type = static_cast<std::uint16_t>(value >> 16);
                }
                else if (kind == 4)
                {
                    // symbol
                }
                else
                {
                    throw std::runtime_error("unsupported SPT token type");
                }

                token.m_byte_length = static_cast<std::uint32_t>(cursor - token_start);
                program.m_tokens.push_back(std::move(token));
            }

            program.m_tokens.shrink_to_fit();
            result.m_programs.push_back(std::move(program));
            at = end;
        }

        if (at < data.size())
        {
            result.m_trailing.assign(data.begin() + static_cast<std::ptrdiff_t>(at), data.end());
        }

        return result;
    }
}
