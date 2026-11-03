#include "assets/assets.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>

bool load_ffon(const fs::path& path, s_ffon_font& font)
{
    auto data = sdbz::read_file(path);
    if (data.size() < 0x40 || std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "FFON")
    {
        std::cerr << "Not an FFON file: " << path << '\n';
        return false;
    }

    // Header fields
    const auto page_count  = sdbz::read_u32_le(std::span(data.data() + 0x10, 4), 0).value();
    const auto last_column = sdbz::read_u32_le(std::span(data.data() + 0x14, 4), 0).value();
    const auto glyph_count = sdbz::read_u32_le(std::span(data.data() + 0x18, 4), 0).value();
    const auto width  = data[0x1C];
    const auto height = data[0x1D];
    const auto row_stride = data[0x1F];

    const auto columns_per_page = last_column + 1;
    const auto char_map_count = page_count * columns_per_page;
    const auto glyph_data_offset = 0xA0 + char_map_count * 2;

    font.width = width;
    font.height = height;
    font.bytes_per_row = row_stride;
    font.glyph_count = glyph_count;
    font.bytes_per_glyph = row_stride * height;

    // Parse char map: slot -> glyph_id (1-based, 0 = none)
    font.char_map.assign(char_map_count, 0);
    for (std::uint32_t slot = 0; slot < char_map_count; ++slot)
    {
        const auto glyph_id = sdbz::read_u16_le(std::span(data.data() + 0xA0 + slot * 2, 2), 0).value();
        font.char_map[slot] = glyph_id;
    }

    // Read glyph bitmaps
    font.glyphs.resize(glyph_count);
    for (std::uint32_t gi = 0; gi < glyph_count; ++gi)
    {
        const auto offset = glyph_data_offset + gi * font.bytes_per_glyph;
        font.glyphs[gi].resize(font.bytes_per_glyph);
        std::memcpy(font.glyphs[gi].data(), data.data() + offset, font.bytes_per_glyph);
    }

    std::cout << "Loaded FFON font: " << path.filename().string()
        << " (" << glyph_count << " glyphs, " << width << "x" << height << ")\n";
    return true;
}

std::pair<int, int> glyph_ink_bounds(const s_ffon_font& font, const std::vector<std::uint8_t>& data)
{
    int left = static_cast<int>(font.width);
    int right = 0;
    for (std::uint32_t y = 0; y < font.height; ++y)
    {
        for (std::uint32_t x = 0; x < font.width; ++x)
        {
            const auto packed = data[y * font.bytes_per_row + (x / 2)];
            const auto value = (x & 1) ? (packed >> 4) : (packed & 0x0F);
            if (value != 0)
            {
                left = std::min(left, static_cast<int>(x));
                right = std::max(right, static_cast<int>(x) + 1);
            }
        }
    }
    if (right == 0) return { 0, 0 };
    return { left, right };
}

GLuint upload_texture(const sdbz::s_texture_image& image)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(image.m_width),
        static_cast<GLsizei>(image.m_height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
        image.m_rgba.data());
    return tex;
}

std::vector<GLuint> load_fpix_textures(const std::vector<std::uint8_t>& data)
{
    std::vector<GLuint> textures;
    if (data.size() < 32 || std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "FPIX")
    {
        return textures;
    }
    const auto directory = sdbz::parse_chunked_container(data, "FPIX");
    for (const auto& chunk : directory.m_chunks)
    {
        if (chunk.magic_string() != "XPIX")
        {
            continue;
        }
        const std::span<const std::uint8_t> chunk_data(data.data() + chunk.m_offset, chunk.m_size);
        try
        {
            const auto image = sdbz::decode_xpix(chunk_data);
            textures.push_back(upload_texture(image));
            std::cout << "Loaded texture " << textures.size() << " from XPIX chunk "
                << chunk.m_offset << " (" << image.m_width << "x" << image.m_height << ")\n";
        }
        catch (const std::exception& exception)
        {
            std::cerr << "XPIX decode failed: " << exception.what() << '\n';
        }
    }
    return textures;
}

std::vector<GLuint> load_fpix_textures(const fs::path& path)
{
    return load_fpix_textures(sdbz::read_file(path));
}

std::string lower_copy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool has_extension(const fs::path& path, const char* extension)
{
    return lower_copy(path.extension().string()) == extension;
}

fs::path directory_of(const fs::path& path)
{
    const auto parent = path.parent_path();
    return parent.empty() ? fs::path(".") : parent;
}

std::vector<fs::path> list_files(const fs::path& dir, const char* extension)
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

std::string part_base_name(const std::string& name)
{
    std::string base = name;
    for (const char* prefix : { "Mesh", "Shape" })
    {
        const std::size_t len = std::strlen(prefix);
        if (base.size() > len && base.compare(0, len, prefix) == 0)
        {
            base.erase(0, len);
            break;
        }
    }
    std::size_t end = base.size();
    while (end > 1 && (std::isupper(static_cast<unsigned char>(base[end - 1])) ||
        std::isdigit(static_cast<unsigned char>(base[end - 1]))))
    {
        --end;
    }
    if (end > 0 && end < base.size() && std::islower(static_cast<unsigned char>(base[end - 1])))
    {
        const std::string suffix = base.substr(end);
        if (suffix != "L" && suffix != "R") base.erase(end);
    }
    return base.empty() ? name : base;
}

void apply_part_visibility(const std::vector<s_part_group>& groups, std::vector<bool>& visible)
{
    std::fill(visible.begin(), visible.end(), false);
    for (const auto& group : groups)
    {
        for (std::size_t i = 0; i < group.m_meshes.size(); ++i)
        {
            visible[group.m_meshes[i]] = (i == group.m_active);
        }
    }
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
