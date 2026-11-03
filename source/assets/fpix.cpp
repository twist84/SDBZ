#include "assets/assets.hpp"

#include <iostream>
#include <span>
#include <string_view>

GLuint upload_texture(const sdbz::s_texture_image& image)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(image.width),
        static_cast<GLsizei>(image.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
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
                << chunk.m_offset << " (" << image.width << "x" << image.height << ")\n";
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