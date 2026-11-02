#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sdbz
{
    [[nodiscard]] bool has_range(std::size_t data_size, std::size_t offset, std::size_t length) noexcept;
    [[nodiscard]] std::optional<std::uint16_t> read_u16_le(std::span<const std::uint8_t> data, std::size_t offset) noexcept;
    [[nodiscard]] std::optional<std::uint32_t> read_u32_le(std::span<const std::uint8_t> data, std::size_t offset) noexcept;
    [[nodiscard]] std::optional<std::int32_t> read_i32_le(std::span<const std::uint8_t> data, std::size_t offset) noexcept;
    [[nodiscard]] std::optional<float> read_f32_le(std::span<const std::uint8_t> data, std::size_t offset) noexcept;
    [[nodiscard]] std::vector<std::uint8_t> read_file(const std::filesystem::path& path);
    void write_file(const std::filesystem::path& path, std::span<const std::uint8_t> data);
    [[nodiscard]] std::string fourcc_to_string(const std::uint8_t* value);
}
