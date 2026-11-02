#include "core/binary_reader.hpp"

#include <bit>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace sdbz
{
    bool has_range(const std::size_t data_size, const std::size_t offset, const std::size_t length) noexcept
    {
        return offset <= data_size && length <= data_size - offset;
    }

    std::optional<std::uint16_t> read_u16_le(const std::span<const std::uint8_t> data, const std::size_t offset) noexcept
    {
        if (!has_range(data.size(), offset, 2))
        {
            return std::nullopt;
        }

        const std::uint16_t value = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(data[offset]) |
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1]) << 8));
        return value;
    }

    std::optional<std::uint32_t> read_u32_le(const std::span<const std::uint8_t> data, const std::size_t offset) noexcept
    {
        if (!has_range(data.size(), offset, 4))
        {
            return std::nullopt;
        }

        return static_cast<std::uint32_t>(data[offset]) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 3]) << 24);
    }

    std::optional<std::int32_t> read_i32_le(const std::span<const std::uint8_t> data, const std::size_t offset) noexcept
    {
        const auto value = read_u32_le(data, offset);
        if (!value)
        {
            return std::nullopt;
        }

        return std::bit_cast<std::int32_t>(*value);
    }

    std::optional<float> read_f32_le(const std::span<const std::uint8_t> data, const std::size_t offset) noexcept
    {
        const auto value = read_u32_le(data, offset);
        if (!value)
        {
            return std::nullopt;
        }

        return std::bit_cast<float>(*value);
    }

    std::vector<std::uint8_t> read_file(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
        {
            throw std::runtime_error("could not open file: " + path.string());
        }

        const std::streampos end = input.tellg();
        if (end < 0)
        {
            throw std::runtime_error("could not determine file size: " + path.string());
        }

        const auto size = static_cast<std::uintmax_t>(end);
        if (size > static_cast<std::uintmax_t>((std::numeric_limits<std::size_t>::max)()))
        {
            throw std::runtime_error("file is too large for this process: " + path.string());
        }

        std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
        input.seekg(0, std::ios::beg);
        if (!result.empty())
        {
            input.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
            if (!input)
            {
                throw std::runtime_error("could not read complete file: " + path.string());
            }
        }

        return result;
    }

    void write_file(const std::filesystem::path& path, const std::span<const std::uint8_t> data)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("could not create file: " + path.string());
        }

        if (!data.empty())
        {
            output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }

        if (!output)
        {
            throw std::runtime_error("could not write complete file: " + path.string());
        }
    }

    std::string fourcc_to_string(const std::uint8_t* value)
    {
        return std::string(reinterpret_cast<const char*>(value), 4);
    }
}
