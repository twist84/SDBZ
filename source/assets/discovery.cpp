#include "assets/assets.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

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