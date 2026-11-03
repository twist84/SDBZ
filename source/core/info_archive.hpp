#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sdbz
{
    inline constexpr std::uint64_t k_archive_sector_size = 2048;
    inline constexpr std::size_t k_info_header_size = 0x30;
    inline constexpr std::size_t k_info_record_size = 0x30;

    struct s_archive_entry
    {
        std::size_t m_index = 0;
        std::string m_name_cp932;
        std::uint32_t m_unknown = 0;
        std::uint32_t m_sector_offset = 0;
        std::uint32_t m_sector_count = 0;
        std::uint32_t m_byte_size = 0;
        bool m_is_directory = false;

        [[nodiscard]] std::uint64_t offset() const noexcept;
        [[nodiscard]] std::uint64_t allocated_size() const noexcept;
    };

    struct s_archive
    {
        std::vector<std::uint8_t> m_encoded_info;
        std::vector<std::uint8_t> m_decoded_info;
        std::vector<s_archive_entry> m_entries;
        std::uint64_t m_used_bytes = 0;

        [[nodiscard]] static s_archive from_encoded_info(
            std::span<const std::uint8_t> encoded_info,
            std::uint64_t game_size = 0);

        [[nodiscard]] static s_archive from_decoded_info(
            std::span<const std::uint8_t> decoded_info,
            std::uint64_t game_size = 0);

        [[nodiscard]] std::size_t extract_all(
            const std::filesystem::path& game_path,
            const std::filesystem::path& destination,
            bool overwrite = false) const;

        void rebuild(
            const std::filesystem::path& info_path,
            const std::filesystem::path& game_path,
            const std::filesystem::path& replacement_root,
            const std::filesystem::path& destination) const;
    };

    [[nodiscard]] std::vector<std::uint8_t> decode_info(std::span<const std::uint8_t> encoded_info);
    [[nodiscard]] std::vector<std::uint8_t> encode_info(std::span<const std::uint8_t> decoded_info);
    [[nodiscard]] bool is_safe_asset_name(std::string_view name) noexcept;
}
