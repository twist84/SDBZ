#include "assets/assets.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>


namespace
{
    struct s_archive_path_parts
    {
        std::string m_directory;
        std::string m_filename;
        std::string m_stem;
        std::string m_extension;
    };

    s_archive_path_parts split_archive_path(std::string_view path)
    {
        std::string normalized(path);
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        normalized = lower_copy(std::move(normalized));

        const std::size_t slash = normalized.find_last_of('/');
        const std::size_t filename_offset =
            slash == std::string::npos ? 0 : slash + 1;
        const std::string filename = normalized.substr(filename_offset);
        const std::size_t dot = filename.find_last_of('.');

        s_archive_path_parts result;
        result.m_directory = slash == std::string::npos
            ? std::string()
            : normalized.substr(0, slash);
        result.m_filename = filename;
        result.m_stem = dot == std::string::npos
            ? filename
            : filename.substr(0, dot);
        result.m_extension = dot == std::string::npos
            ? std::string()
            : filename.substr(dot);
        return result;
    }
}

std::string lower_copy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool has_extension(const fs::path& path, std::string_view extension)
{
    return lower_copy(path.extension().string()) == extension;
}

fs::path directory_of(const fs::path& path)
{
    const auto parent = path.parent_path();
    return parent.empty() ? fs::path(".") : parent;
}

std::vector<fs::path> list_files(const fs::path& dir, std::string_view extension)
{
    std::vector<fs::path> result;
    std::error_code ec;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec))
    {
        std::error_code entry_ec;
        if (it->is_regular_file(entry_ec) && has_extension(it->path(), extension))
        {
            result.push_back(it->path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::string character_prefix(const std::string& stem)
{
    std::string prefix = lower_copy(stem);
    std::size_t end = prefix.size();
    while (end > 0 && std::isalpha(static_cast<unsigned char>(prefix[end - 1]))) --end;
    if (end > 0 && end < prefix.size() && std::isdigit(static_cast<unsigned char>(prefix[end - 1])))
    {
        prefix.erase(end);
    }
    return prefix;
}

fs::path find_texture_for(const fs::path& model_path)
{
    const auto candidates = list_files(directory_of(model_path), ".pix");
    const std::string stem = lower_copy(model_path.stem().string());
    for (const auto& candidate : candidates)
    {
        if (lower_copy(candidate.stem().string()) == stem) return candidate;
    }
    const std::string prefix = character_prefix(stem);
    for (const auto& candidate : candidates)
    {
        if (lower_copy(candidate.stem().string()).rfind(prefix, 0) == 0) return candidate;
    }
    return {};
}

std::vector<fs::path> find_motions_for(const fs::path& model_path)
{
    const auto all = list_files(directory_of(model_path), ".pmt");
    const std::string prefix = character_prefix(model_path.stem().string());
    std::vector<fs::path> matching;
    for (const auto& candidate : all)
    {
        if (lower_copy(candidate.stem().string()).rfind(prefix, 0) == 0) matching.push_back(candidate);
    }
    return matching.empty() ? all : matching;
}

const sdbz::s_archive_entry* find_archive_texture_for(
    const sdbz::s_archive& archive, const std::string_view model_name)
{
    const s_archive_path_parts model = split_archive_path(model_name);
    const std::string prefix = character_prefix(model.m_stem);
    const sdbz::s_archive_entry* prefix_match = nullptr;
    std::string prefix_match_filename;

    for (const auto& entry : archive.m_entries)
    {
        if (entry.m_is_directory)
        {
            continue;
        }
        const s_archive_path_parts candidate = split_archive_path(entry.m_name_cp932);
        if (candidate.m_directory != model.m_directory ||
            candidate.m_extension != ".pix")
        {
            continue;
        }
        if (candidate.m_stem == model.m_stem)
        {
            return &entry;
        }
        if (!prefix.empty() && candidate.m_stem.rfind(prefix, 0) == 0 &&
            (prefix_match == nullptr || candidate.m_filename < prefix_match_filename))
        {
            prefix_match = &entry;
            prefix_match_filename = candidate.m_filename;
        }
    }

    return prefix_match;
}

std::vector<const sdbz::s_archive_entry*> find_archive_motions_for(
    const sdbz::s_archive& archive, const std::string_view model_name)
{
    const s_archive_path_parts model = split_archive_path(model_name);
    const std::string prefix = character_prefix(model.m_stem);
    std::vector<const sdbz::s_archive_entry*> all;
    std::vector<const sdbz::s_archive_entry*> matching;

    for (const auto& entry : archive.m_entries)
    {
        if (entry.m_is_directory)
        {
            continue;
        }
        const s_archive_path_parts candidate = split_archive_path(entry.m_name_cp932);
        if (candidate.m_directory != model.m_directory ||
            candidate.m_extension != ".pmt")
        {
            continue;
        }

        all.push_back(&entry);
        if (!prefix.empty() && candidate.m_stem.rfind(prefix, 0) == 0)
        {
            matching.push_back(&entry);
        }
    }

    const auto sort_entries = [](auto& entries)
    {
        std::sort(entries.begin(), entries.end(), [](const auto* left, const auto* right)
        {
            return lower_copy(left->m_name_cp932) < lower_copy(right->m_name_cp932);
        });
    };
    sort_entries(all);
    sort_entries(matching);
    return matching.empty() ? all : matching;
}

fs::path find_font_near(const fs::path& start)
{
    std::error_code ec;
    std::vector<fs::path> roots;
    fs::path dir = fs::absolute(start, ec);
    for (int level = 0; level < 4 && !dir.empty(); ++level)
    {
        roots.push_back(dir);
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    roots.push_back(fs::current_path(ec));
    for (const auto& root : roots)
    {
        std::error_code scan_ec;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, scan_ec), end;
             !scan_ec && it != end; it.increment(scan_ec))
        {
            if (it.depth() >= 3) it.disable_recursion_pending();
            std::error_code entry_ec;
            if (it->is_regular_file(entry_ec) && has_extension(it->path(), ".fon")) return it->path();
        }
    }
    return {};
}