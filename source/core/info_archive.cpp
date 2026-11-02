#include "core/info_archive.hpp"

#include "core/binary_reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace sdbz
{
    namespace
    {
        constexpr std::array<std::uint8_t, 15> k_signature =
        {
            'w', 'w', 'w', '.', 'a', 'r', 'i', 'k', 'a', '.', 'c', 'o', '.', 'j', 'p'
        };

        [[nodiscard]] std::uint8_t swap_nibbles(const std::uint8_t value) noexcept
        {
            return static_cast<std::uint8_t>((value << 4) | (value >> 4));
        }

        void validate_info_signature(const std::span<const std::uint8_t> data)
        {
            if (data.size() < k_info_header_size)
            {
                throw std::runtime_error("INFO.DAT is smaller than its 0x30-byte header");
            }

            if (!std::equal(k_signature.begin(), k_signature.end(), data.begin() + 1))
            {
                throw std::runtime_error("INFO.DAT signature is not www.arika.co.jp");
            }
        }

        [[nodiscard]] std::string ascii_fold(const std::string& value)
        {
            std::string result = value;
            for (char& ch : result)
            {
                const auto byte = static_cast<unsigned char>(ch);
                if (byte >= 'A' && byte <= 'Z')
                {
                    ch = static_cast<char>(byte + ('a' - 'A'));
                }
            }
            return result;
        }

        [[nodiscard]] std::filesystem::path path_from_cp932(const std::string& name)
        {
#if defined(_WIN32)
            if (name.empty())
            {
                return {};
            }

            const int required = MultiByteToWideChar(932, 0, name.data(), static_cast<int>(name.size()), nullptr, 0);
            if (required <= 0)
            {
                throw std::runtime_error("could not convert a CP932 asset filename");
            }

            std::wstring wide(static_cast<std::size_t>(required), L'\0');
            if (MultiByteToWideChar(932, 0, name.data(), static_cast<int>(name.size()), wide.data(), required) <= 0)
            {
                throw std::runtime_error("could not convert a CP932 asset filename");
            }
            return std::filesystem::path(wide);
#else
            // POSIX paths are byte strings. Keeping the original CP932 bytes is lossless.
            return std::filesystem::path(name);
#endif
        }

        [[nodiscard]] bool path_is_within(const std::filesystem::path& root, const std::filesystem::path& candidate)
        {
            auto root_it = root.begin();
            auto candidate_it = candidate.begin();
            for (; root_it != root.end(); ++root_it, ++candidate_it)
            {
                if (candidate_it == candidate.end() || *root_it != *candidate_it)
                {
                    return false;
                }
            }
            return true;
        }

        void store_u32_le(std::vector<std::uint8_t>& data, const std::size_t offset, const std::uint32_t value)
        {
            if (offset > data.size() || data.size() - offset < 4)
            {
                throw std::runtime_error("INFO.DAT field write is out of range");
            }
            data[offset + 0] = static_cast<std::uint8_t>(value);
            data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
            data[offset + 2] = static_cast<std::uint8_t>(value >> 16);
            data[offset + 3] = static_cast<std::uint8_t>(value >> 24);
        }

        void copy_exact(std::ifstream& input, std::ofstream& output, std::uint64_t count)
        {
            std::array<char, 4096> buffer{};
            while (count > 0)
            {
                const std::size_t amount = static_cast<std::size_t>(std::min<std::uint64_t>(count, buffer.size()));
                input.read(buffer.data(), static_cast<std::streamsize>(amount));
                if (input.gcount() != static_cast<std::streamsize>(amount))
                {
                    throw std::runtime_error("unexpected end of GAME.DAT while extracting an asset");
                }
                output.write(buffer.data(), static_cast<std::streamsize>(amount));
                if (!output)
                {
                    throw std::runtime_error("failed while writing an extracted asset");
                }
                count -= amount;
            }
        }
    }

    std::uint64_t s_archive_entry::offset() const noexcept
    {
        return static_cast<std::uint64_t>(m_sector_offset) * k_archive_sector_size;
    }

    std::uint64_t s_archive_entry::allocated_size() const noexcept
    {
        return static_cast<std::uint64_t>(m_sector_count) * k_archive_sector_size;
    }

    std::vector<std::uint8_t> decode_info(const std::span<const std::uint8_t> encoded_info)
    {
        validate_info_signature(encoded_info);
        std::vector<std::uint8_t> decoded(encoded_info.begin(), encoded_info.end());
        const std::array<std::uint8_t, 16> key = [&]()
        {
            std::array<std::uint8_t, 16> result{};
            std::copy_n(encoded_info.begin(), result.size(), result.begin());
            return result;
        }();

        for (std::size_t i = 16; i < decoded.size(); ++i)
        {
            decoded[i] = static_cast<std::uint8_t>(static_cast<std::uint8_t>(~swap_nibbles(encoded_info[i])) - key[i & 15]);
        }
        return decoded;
    }

    std::vector<std::uint8_t> encode_info(const std::span<const std::uint8_t> decoded_info)
    {
        validate_info_signature(decoded_info);
        std::vector<std::uint8_t> encoded(decoded_info.begin(), decoded_info.end());
        std::array<std::uint8_t, 16> key{};
        std::copy_n(decoded_info.begin(), key.size(), key.begin());

        for (std::size_t i = 16; i < encoded.size(); ++i)
        {
            const std::uint8_t inverted = static_cast<std::uint8_t>(~static_cast<std::uint8_t>(decoded_info[i] + key[i & 15]));
            encoded[i] = swap_nibbles(inverted);
        }
        return encoded;
    }

    bool is_safe_asset_name(const std::string& name) noexcept
    {
        if (name.empty() || name.front() == '/' || name.front() == '\\' || name.find(':') != std::string::npos)
        {
            return false;
        }

        std::string normalized = name;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        std::size_t begin = 0;
        while (begin <= normalized.size())
        {
            const std::size_t end = normalized.find('/', begin);
            const std::string_view component(normalized.data() + begin,
                (end == std::string::npos ? normalized.size() : end) - begin);
            if (component == ".." || component.empty())
            {
                return false;
            }
            for (const char raw_ch : component)
            {
                const auto ch = static_cast<unsigned char>(raw_ch);
                if (ch < 0x20 || ch == 0x7F)
                {
                    return false;
                }
            }

            if (end == std::string::npos)
            {
                break;
            }
            begin = end + 1;
        }

        return true;
    }

    s_archive s_archive::from_encoded_info(
        const std::span<const std::uint8_t> encoded_info,
        const std::uint64_t game_size)
    {
        s_archive result;
        result.m_encoded_info.assign(encoded_info.begin(), encoded_info.end());
        result.m_decoded_info = decode_info(encoded_info);

        // Parse into a temporary archive using the common decoded-table path.
        s_archive parsed = from_decoded_info(result.m_decoded_info, game_size);
        parsed.m_encoded_info = std::move(result.m_encoded_info);
        return parsed;
    }

    s_archive s_archive::from_decoded_info(
        const std::span<const std::uint8_t> decoded_info,
        const std::uint64_t game_size)
    {
        validate_info_signature(decoded_info);
        const auto count_value = read_u32_le(decoded_info, 0x2C);
        if (!count_value)
        {
            throw std::runtime_error("INFO.DAT is missing its file count");
        }

        const std::size_t count = *count_value;
        if (count > (decoded_info.size() - k_info_header_size) / k_info_record_size)
        {
            throw std::runtime_error("INFO.DAT entry count exceeds the decoded table size");
        }

        s_archive result;
        result.m_decoded_info.assign(decoded_info.begin(), decoded_info.end());
        result.m_entries.reserve(count);

        std::vector<std::string> names;
        names.reserve(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            const std::size_t at = k_info_header_size + index * k_info_record_size;
            const auto name_begin = decoded_info.begin() + static_cast<std::ptrdiff_t>(at);
            const auto name_end = std::find(name_begin, name_begin + 32, 0);
            const std::string name(name_begin, name_end);
            if (!is_safe_asset_name(name))
            {
                throw std::runtime_error("unsafe asset path in INFO.DAT entry " + std::to_string(index));
            }

            const std::string folded = ascii_fold(name);
            if (std::find(names.begin(), names.end(), folded) != names.end())
            {
                throw std::runtime_error("duplicate/case-colliding asset name in INFO.DAT: " + name);
            }
            names.push_back(folded);

            const auto unknown = read_u32_le(decoded_info, at + 32);
            const auto sector = read_u32_le(decoded_info, at + 36);
            const auto sectors = read_u32_le(decoded_info, at + 40);
            const auto byte_size = read_u32_le(decoded_info, at + 44);
            if (!unknown || !sector || !sectors || !byte_size)
            {
                throw std::runtime_error("truncated INFO.DAT entry " + std::to_string(index));
            }

            const std::uint64_t needed_sectors = (static_cast<std::uint64_t>(*byte_size) + k_archive_sector_size - 1) / k_archive_sector_size;
            if (*sectors < needed_sectors)
            {
                throw std::runtime_error("asset allocation is smaller than its byte size: " + name);
            }

            result.m_entries.push_back(s_archive_entry
            {
                index,
                name,
                *unknown,
                *sector,
                *sectors,
                *byte_size
            });
        }

        std::vector<const s_archive_entry*> sorted;
        sorted.reserve(result.m_entries.size());
        for (const auto& entry : result.m_entries)
        {
            sorted.push_back(&entry);
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto* left, const auto* right)
        {
            return left->offset() < right->offset();
        });

        std::uint64_t last_end = 0;
        for (const auto* entry : sorted)
        {
            if (entry->offset() < last_end)
            {
                throw std::runtime_error("INFO.DAT contains overlapping GAME.DAT extents");
            }
            if (entry->allocated_size() > (std::numeric_limits<std::uint64_t>::max)() - entry->offset())
            {
                throw std::runtime_error("GAME.DAT extent overflows 64-bit offset space");
            }
            last_end = entry->offset() + entry->allocated_size();
        }

        if (game_size != 0 && last_end > game_size)
        {
            throw std::runtime_error("GAME.DAT is too small for the INFO.DAT index");
        }

        result.m_used_bytes = last_end;
        return result;
    }

    std::size_t s_archive::extract_all(
        const std::filesystem::path& game_path,
        const std::filesystem::path& destination,
        const bool overwrite) const
    {
        std::error_code error;
        std::filesystem::create_directories(destination, error);
        if (error)
        {
            throw std::runtime_error("could not create extraction directory: " + error.message());
        }

        const auto root = std::filesystem::weakly_canonical(destination, error);
        if (error)
        {
            throw std::runtime_error("could not resolve extraction directory: " + error.message());
        }

        std::ifstream game(game_path, std::ios::binary);
        if (!game)
        {
            throw std::runtime_error("could not open GAME.DAT: " + game_path.string());
        }

        const auto game_size = std::filesystem::file_size(game_path, error);
        if (error || m_used_bytes > game_size)
        {
            throw std::runtime_error("GAME.DAT is too small for this INFO.DAT index");
        }

        struct s_extract_target
        {
            const s_archive_entry* m_entry;
            std::filesystem::path m_path;
        };
        std::vector<s_extract_target> targets;
        targets.reserve(m_entries.size());

        for (const auto& entry : m_entries)
        {
            std::string normalized = entry.m_name_cp932;
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            const auto relative = path_from_cp932(normalized);
            if (relative.is_absolute())
            {
                throw std::runtime_error("absolute path found in archive entry: " + entry.m_name_cp932);
            }

            const auto output_path = root / relative;
            const auto parent = output_path.parent_path();
            std::filesystem::create_directories(parent, error);
            if (error)
            {
                throw std::runtime_error("could not create output directory: " + error.message());
            }

            const auto resolved_parent = std::filesystem::weakly_canonical(parent, error);
            if (error || !path_is_within(root, resolved_parent))
            {
                throw std::runtime_error("archive path escapes extraction directory: " + entry.m_name_cp932);
            }
            const auto target_status = std::filesystem::symlink_status(output_path, error);
            if (error && error != std::errc::no_such_file_or_directory)
            {
                throw std::runtime_error("could not inspect output path: " + error.message());
            }
            if (!error && std::filesystem::is_symlink(target_status))
            {
                throw std::runtime_error("refusing to write through an output symlink: " + output_path.string());
            }
            error.clear();
            if (std::filesystem::exists(output_path, error) && !overwrite)
            {
                throw std::runtime_error("output exists; refusing to overwrite: " + output_path.string());
            }
            if (error)
            {
                throw std::runtime_error("could not inspect output path: " + error.message());
            }

            targets.push_back({ &entry, output_path });
        }

        for (const auto& target : targets)
        {
            game.clear();
            game.seekg(static_cast<std::streamoff>(target.m_entry->offset()), std::ios::beg);
            if (!game)
            {
                throw std::runtime_error("could not seek to GAME.DAT asset: " + target.m_entry->m_name_cp932);
            }

            std::ofstream output(target.m_path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                throw std::runtime_error("could not create extracted asset: " + target.m_path.string());
            }
            copy_exact(game, output, target.m_entry->m_byte_size);
        }

        return targets.size();
    }

    void s_archive::rebuild(
        const std::filesystem::path& info_path,
        const std::filesystem::path& game_path,
        const std::filesystem::path& replacement_root,
        const std::filesystem::path& destination) const
    {
        std::error_code error;
        const auto input_info = std::filesystem::weakly_canonical(info_path, error);
        if (error)
        {
            throw std::runtime_error("could not resolve input INFO.DAT path: " + error.message());
        }
        const auto input_game = std::filesystem::weakly_canonical(game_path, error);
        if (error)
        {
            throw std::runtime_error("could not resolve input GAME.DAT path: " + error.message());
        }

        std::filesystem::create_directories(destination, error);
        if (error)
        {
            throw std::runtime_error("could not create rebuild output directory: " + error.message());
        }
        const auto output_root = std::filesystem::weakly_canonical(destination, error);
        if (error)
        {
            throw std::runtime_error("could not resolve rebuild output directory: " + error.message());
        }
        const auto output_info = output_root / "INFO.DAT";
        const auto output_game = output_root / "GAME.DAT";
        if (output_info == input_info || output_game == input_game ||
            output_info == input_game || output_game == input_info)
        {
            throw std::runtime_error("rebuild output must not overwrite either input archive file");
        }
        if (std::filesystem::exists(output_info) || std::filesystem::exists(output_game))
        {
            throw std::runtime_error("rebuild output INFO.DAT or GAME.DAT already exists");
        }

        std::filesystem::path replacement_base;
        if (!replacement_root.empty())
        {
            if (!std::filesystem::exists(replacement_root) || !std::filesystem::is_directory(replacement_root))
            {
                throw std::runtime_error("replacement root is not a directory: " + replacement_root.string());
            }
            replacement_base = std::filesystem::weakly_canonical(replacement_root, error);
            if (error)
            {
                throw std::runtime_error("could not resolve replacement directory: " + error.message());
            }
        }

        const auto game_size = std::filesystem::file_size(game_path, error);
        if (error || game_size < m_used_bytes)
        {
            throw std::runtime_error("input GAME.DAT is too small for this INFO.DAT index");
        }

        struct s_rebuild_entry
        {
            const s_archive_entry* m_entry = nullptr;
            std::filesystem::path m_replacement;
            std::uint32_t m_sector = 0;
            std::uint32_t m_sector_count = 0;
            std::uint32_t m_byte_size = 0;
        };

        std::vector<s_rebuild_entry> plan;
        plan.reserve(m_entries.size());
        std::uint64_t next_sector = 0;
        for (const auto& entry : m_entries)
        {
            std::filesystem::path replacement;
            std::uint64_t byte_size = entry.m_byte_size;
            if (!replacement_base.empty())
            {
                auto relative = entry.m_name_cp932;
                std::replace(relative.begin(), relative.end(), '\\', '/');
                const auto candidate = replacement_base / path_from_cp932(relative);
                if (std::filesystem::exists(candidate, error))
                {
                    const auto status = std::filesystem::symlink_status(candidate, error);
                    if (error || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status))
                    {
                        throw std::runtime_error("replacement is not a regular non-symlink file: " + candidate.string());
                    }
                    const auto resolved = std::filesystem::weakly_canonical(candidate, error);
                    if (error || !path_is_within(replacement_base, resolved))
                    {
                        throw std::runtime_error("replacement path escapes replacement root: " + candidate.string());
                    }
                    replacement = candidate;
                    byte_size = std::filesystem::file_size(replacement, error);
                    if (error)
                    {
                        throw std::runtime_error("could not determine replacement size: " + replacement.string());
                    }
                }
                else if (error)
                {
                    throw std::runtime_error("could not inspect replacement: " + candidate.string());
                }
            }

            if (byte_size > (std::numeric_limits<std::uint32_t>::max)())
            {
                throw std::runtime_error("replacement exceeds the INFO.DAT 32-bit size field: " + entry.m_name_cp932);
            }
            const std::uint64_t sector_count = (byte_size + k_archive_sector_size - 1) / k_archive_sector_size;
            if (next_sector > (std::numeric_limits<std::uint32_t>::max)() ||
                sector_count > (std::numeric_limits<std::uint32_t>::max)() ||
                next_sector + sector_count > (std::numeric_limits<std::uint32_t>::max)())
            {
                throw std::runtime_error("rebuilt GAME.DAT exceeds the INFO.DAT sector fields");
            }

            plan.push_back(s_rebuild_entry
            {
                &entry,
                replacement,
                static_cast<std::uint32_t>(next_sector),
                static_cast<std::uint32_t>(sector_count),
                static_cast<std::uint32_t>(byte_size)
            });
            next_sector += sector_count;
        }

        const std::uint64_t new_used_bytes = next_sector * k_archive_sector_size;
        if (new_used_bytes > game_size)
        {
            throw std::runtime_error("replacement assets exceed the original GAME.DAT capacity");
        }

        auto decoded_info = m_decoded_info;
        for (const auto& item : plan)
        {
            const std::size_t offset = k_info_header_size + item.m_entry->m_index * k_info_record_size + 36;
            store_u32_le(decoded_info, offset, item.m_sector);
            store_u32_le(decoded_info, offset + 4, item.m_sector_count);
            store_u32_le(decoded_info, offset + 8, item.m_byte_size);
        }
        const auto encoded_info = encode_info(decoded_info);

        const auto game_temp = output_root / ".GAME.DAT.tmp";
        const auto info_temp = output_root / ".INFO.DAT.tmp";
        if (std::filesystem::exists(game_temp) || std::filesystem::exists(info_temp))
        {
            throw std::runtime_error("temporary rebuild output already exists");
        }

        std::ifstream source_game(game_path, std::ios::binary);
        if (!source_game)
        {
            throw std::runtime_error("could not open source GAME.DAT: " + game_path.string());
        }

        try
        {
            std::ofstream rebuilt_game(game_temp, std::ios::binary | std::ios::trunc);
            if (!rebuilt_game)
            {
                throw std::runtime_error("could not create temporary GAME.DAT");
            }

            for (const auto& item : plan)
            {
                source_game.clear();
                if (item.m_replacement.empty())
                {
                    source_game.seekg(static_cast<std::streamoff>(item.m_entry->offset()), std::ios::beg);
                    if (!source_game)
                    {
                        throw std::runtime_error("could not seek to original asset: " + item.m_entry->m_name_cp932);
                    }
                    copy_exact(source_game, rebuilt_game, item.m_byte_size);
                }
                else
                {
                    std::ifstream replacement(item.m_replacement, std::ios::binary);
                    if (!replacement)
                    {
                        throw std::runtime_error("could not open replacement: " + item.m_replacement.string());
                    }
                    copy_exact(replacement, rebuilt_game, item.m_byte_size);
                    if (replacement.peek() != std::ifstream::traits_type::eof())
                    {
                        throw std::runtime_error("replacement changed size while rebuilding: " + item.m_replacement.string());
                    }
                }

                const std::uint64_t padding =
                    static_cast<std::uint64_t>(item.m_sector_count) * k_archive_sector_size - item.m_byte_size;
                if (padding > 0 && item.m_replacement.empty() &&
                    item.m_byte_size == item.m_entry->m_byte_size &&
                    item.m_sector_count == item.m_entry->m_sector_count)
                {
                    source_game.clear();
                    source_game.seekg(static_cast<std::streamoff>(item.m_entry->offset() + item.m_byte_size), std::ios::beg);
                    if (!source_game)
                    {
                        throw std::runtime_error("could not seek to original asset padding");
                    }
                    copy_exact(source_game, rebuilt_game, padding);
                }
                else if (padding > 0)
                {
                    std::array<char, 4096> zeroes{};
                    std::uint64_t remaining = padding;
                    while (remaining > 0)
                    {
                        const auto amount = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, zeroes.size()));
                        rebuilt_game.write(zeroes.data(), amount);
                        if (!rebuilt_game)
                        {
                            throw std::runtime_error("failed while writing rebuilt GAME.DAT padding");
                        }
                        remaining -= static_cast<std::uint64_t>(amount);
                    }
                }
            }

            const auto current_position = rebuilt_game.tellp();
            if (current_position < 0 || static_cast<std::uint64_t>(current_position) > game_size)
            {
                throw std::runtime_error("rebuilt GAME.DAT exceeds original archive size");
            }
            const auto tail_size = game_size - static_cast<std::uint64_t>(current_position);
            source_game.clear();
            source_game.seekg(static_cast<std::streamoff>(current_position), std::ios::beg);
            if (!source_game)
            {
                throw std::runtime_error("could not seek to original GAME.DAT tail");
            }
            copy_exact(source_game, rebuilt_game, tail_size);
            rebuilt_game.flush();
            if (!rebuilt_game)
            {
                throw std::runtime_error("failed while finalizing rebuilt GAME.DAT");
            }
            rebuilt_game.close();

            write_file(info_temp, encoded_info);
            std::filesystem::rename(game_temp, output_game);
            std::filesystem::rename(info_temp, output_info);
        }
        catch (...)
        {
            std::filesystem::remove(game_temp, error);
            std::filesystem::remove(info_temp, error);
            throw;
        }
    }
}






