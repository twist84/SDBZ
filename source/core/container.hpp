#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sdbz
{
    struct s_container_chunk
    {
        std::array<std::uint8_t, 4> m_magic{};
        std::uint32_t m_offset = 0;
        std::uint32_t m_size = 0;

        [[nodiscard]] std::string magic_string() const;
    };

    struct s_chunked_container
    {
        std::array<std::uint8_t, 4> m_magic{};
        std::uint32_t m_file_size = 0;
        std::uint32_t m_chunk_count = 0;
        std::uint32_t m_format = 0;
        std::vector<s_container_chunk> m_chunks;
    };

    [[nodiscard]] s_chunked_container parse_chunked_container(
        std::span<const std::uint8_t> data,
        std::string_view expected_magic = {});
}
