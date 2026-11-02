#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace sdbz
{
    enum class e_texture_format
    {
        rgba32,
        indexed4,
        indexed8
    };

    struct s_texture_image
    {
        std::uint32_t m_width = 0;
        std::uint32_t m_height = 0;
        e_texture_format m_format = e_texture_format::rgba32;
        std::vector<std::uint8_t> m_rgba;
    };

    [[nodiscard]] s_texture_image decode_tim2(std::span<const std::uint8_t> data);
    [[nodiscard]] s_texture_image decode_xpix(std::span<const std::uint8_t> data);
    [[nodiscard]] s_texture_image decode_first_fpix_texture(std::span<const std::uint8_t> data);
    void write_bmp_rgba(const std::filesystem::path& path, const s_texture_image& image);
}
