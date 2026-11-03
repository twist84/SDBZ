#include "core/cht.hpp"
#include "core/binary_reader.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace sdbz
{
    namespace
    {
        void align_cursor(std::size_t& cursor)
        {
            cursor += (-cursor) % 16;
        }
    }

    s_cht parse_cht(std::span<const std::uint8_t> data)
    {
        if (data.size() < 20)
        {
            throw std::runtime_error("file too small for CHT header");
        }

        if (!(data[0] == 0 && data[1] == 0 && data[2] == 1 && data[3] == 0 &&
              data[4] == 'C' && data[5] == 'H' && data[6] == 'T' && data[7] == ' '))
        {
            throw std::runtime_error("bad CHT magic");
        }

        s_cht result;
        const std::uint32_t count = read_u32_le(data, 16).value_or(0);
        if (count > 0x10000)
        {
            throw std::runtime_error("invalid CHT name count");
        }

        std::size_t cursor = 20;
        result.m_names.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (cursor + 32 > data.size())
            {
                throw std::runtime_error("truncated CHT name");
            }
            char buf[33] = {};
            std::memcpy(buf, data.data() + cursor, 32);
            result.m_names.emplace_back(buf);
            cursor += 32;
        }

        if (cursor + 72 > data.size())
        {
            throw std::runtime_error("truncated CHT flags");
        }
        for (std::size_t i = 0; i < 18; ++i)
        {
            result.m_flags[i] = read_u32_le(data, cursor + i * 4).value_or(0);
        }
        cursor += 72;

        for (std::uint32_t flag : result.m_flags)
        {
            align_cursor(cursor);
            if (flag)
            {
                if (cursor + 32 > data.size())
                {
                    throw std::runtime_error("truncated CHT flag block");
                }
                cursor += 32;
            }
        }

        if (cursor + 4000 > data.size())
        {
            throw std::runtime_error("truncated CHT directory");
        }
        result.m_directory.assign(data.begin() + static_cast<std::ptrdiff_t>(cursor),
            data.begin() + static_cast<std::ptrdiff_t>(cursor + 4000));
        cursor += 4000;

        while (cursor < data.size())
        {
            if (cursor + 12 > data.size())
            {
                throw std::runtime_error("truncated CHT action");
            }
            const std::uint32_t logical_id = read_u32_le(data, cursor).value_or(0);
            const std::uint32_t variants = read_u32_le(data, cursor + 4).value_or(0);
            cursor += 12;

            if (logical_id >= 1000)
            {
                throw std::runtime_error("invalid CHT logical id");
            }
            if (variants == 0 || cursor + variants * 136 > data.size())
            {
                throw std::runtime_error("invalid CHT variant count");
            }

            const std::size_t base = cursor;
            cursor += variants * 136;

            s_cht_action action;
            action.m_motion_id = logical_id;
            action.m_variants.reserve(variants);

            for (std::uint32_t v = 0; v < variants; ++v)
            {
                const std::size_t at = base + v * 136;
                s_cht_variant variant;
                variant.m_name_index = read_u16_le(data, at + 4).value_or(0);
                variant.m_resource_bank = read_u16_le(data, at + 6).value_or(0);

                if (variant.m_name_index < result.m_names.size())
                {
                    variant.m_motion_name = result.m_names[variant.m_name_index];
                }

                // Walk child blocks, advancing cursor, matching arika_cht.py.
                for (std::size_t field = 8; field < 72; field += 4)
                {
                if (read_u32_le(data, at + field).value_or(0) != 0)
                {
                    if (cursor + 68 > data.size())
                    {
                        throw std::runtime_error("truncated CHT type-68 block");
                    }
                    const std::size_t child = cursor;
                    cursor += 68;
                    align_cursor(cursor);
                    for (std::size_t subfield = 36; subfield < 68; subfield += 4)
                        {
                            if (read_u32_le(data, child + subfield).value_or(0) != 0)
                            {
                                if (cursor + 32 > data.size())
                                {
                                    throw std::runtime_error("truncated CHT type-32 block");
                                }
                                cursor += 32;
                            }
                        }
                    }
                }

                for (std::size_t field = 104; field < 136; field += 4)
                {
                    if (read_u32_le(data, at + field).value_or(0) != 0)
                    {
                        if (cursor + 36 > data.size())
                        {
                            throw std::runtime_error("truncated CHT type-36 block");
                        }
                        const std::size_t child = cursor;
                        cursor += 36;
                        align_cursor(cursor);
                        for (std::size_t subfield = 4; subfield < 36; subfield += 4)
                        {
                            if (read_u32_le(data, child + subfield).value_or(0) != 0)
                            {
                                if (cursor + 32 > data.size())
                                {
                                    throw std::runtime_error("truncated CHT type-32 block");
                                }
                                cursor += 32;
                            }
                        }
                    }
                }

                for (std::size_t field = 72; field < 104; field += 4)
                {
                    if (read_u32_le(data, at + field).value_or(0) != 0)
                    {
                        if (cursor + 12 > data.size())
                        {
                            throw std::runtime_error("truncated CHT type-12 block");
                        }
                        cursor += 12;
                    }
                }

                action.m_variants.push_back(std::move(variant));
            }

            result.m_actions.push_back(std::move(action));
        }

        return result;
    }
}
