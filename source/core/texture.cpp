#include "core/texture.hpp"

#include "core/binary_reader.hpp"
#include "core/container.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace sdbz
{
    namespace
    {
        [[nodiscard]] std::uint8_t expand_alpha(const std::uint8_t value) noexcept
        {
            return static_cast<std::uint8_t>(std::min<unsigned int>(255, static_cast<unsigned int>(value) * 2));
        }

        void append_rgba(std::vector<std::uint8_t>& output, const std::uint8_t* color)
        {
            output.push_back(color[0]);
            output.push_back(color[1]);
            output.push_back(color[2]);
            output.push_back(expand_alpha(color[3]));
        }

        void write_u16_le(std::vector<std::uint8_t>& output, const std::size_t offset, const std::uint16_t value)
        {
            output[offset + 0] = static_cast<std::uint8_t>(value);
            output[offset + 1] = static_cast<std::uint8_t>(value >> 8);
        }

        void write_u32_le(std::vector<std::uint8_t>& output, const std::size_t offset, const std::uint32_t value)
        {
            output[offset + 0] = static_cast<std::uint8_t>(value);
            output[offset + 1] = static_cast<std::uint8_t>(value >> 8);
            output[offset + 2] = static_cast<std::uint8_t>(value >> 16);
            output[offset + 3] = static_cast<std::uint8_t>(value >> 24);
        }
    }

    s_texture_image decode_tim2(const std::span<const std::uint8_t> data)
    {
        if (data.size() < 16 || fourcc_to_string(data.data()) != "TIM2")
        {
            throw std::runtime_error("expected TIM2 texture");
        }

        const std::uint8_t format_value = data[5];
        const auto picture_count = read_u16_le(data, 6);
        if (format_value > 1 || !picture_count || *picture_count != 1)
        {
            throw std::runtime_error("unsupported TIM2 container format or picture count");
        }

        const std::size_t picture_offset = format_value == 1 ? 128 : 16;
        if (!has_range(data.size(), picture_offset, 48))
        {
            throw std::runtime_error("truncated TIM2 picture header");
        }

        const auto total_size = read_u32_le(data, picture_offset);
        const auto clut_size = read_u32_le(data, picture_offset + 4);
        const auto image_size = read_u32_le(data, picture_offset + 8);
        const auto header_size = read_u16_le(data, picture_offset + 12);
        const auto colour_count = read_u16_le(data, picture_offset + 14);
        const auto width_value = read_u16_le(data, picture_offset + 20);
        const auto height_value = read_u16_le(data, picture_offset + 22);
        if (!total_size || !clut_size || !image_size || !header_size || !colour_count || !width_value || !height_value)
        {
            throw std::runtime_error("truncated TIM2 picture fields");
        }

        const std::uint8_t mipmaps = data[picture_offset + 17];
        const std::uint8_t clut_type = data[picture_offset + 18];
        const std::uint8_t image_type = data[picture_offset + 19];
        const std::uint32_t width = *width_value;
        const std::uint32_t height = *height_value;
        if (mipmaps != 1 || width == 0 || height == 0 || *header_size < 48)
        {
            throw std::runtime_error("unsupported TIM2 picture layout");
        }

        const std::uint64_t payload_size = static_cast<std::uint64_t>(*header_size) + *image_size + *clut_size;
        if (payload_size > *total_size || !has_range(data.size(), picture_offset, *total_size))
        {
            throw std::runtime_error("TIM2 picture payload exceeds its declared extent");
        }

        const std::size_t image_offset = picture_offset + *header_size;
        const std::size_t palette_offset = image_offset + *image_size;
        const std::uint64_t pixel_count64 = static_cast<std::uint64_t>(width) * height;
        if (pixel_count64 > (std::numeric_limits<std::size_t>::max)() / 4)
        {
            throw std::runtime_error("TIM2 dimensions exceed addressable memory");
        }
        const std::size_t pixel_count = static_cast<std::size_t>(pixel_count64);

        s_texture_image result;
        result.m_width = width;
        result.m_height = height;
        result.m_rgba.reserve(pixel_count * 4);

        if (image_type == 3)
        {
            if (*image_size != pixel_count * 4)
            {
                throw std::runtime_error("unexpected TIM2 RGBA32 image size");
            }
            result.m_format = e_texture_format::rgba32;
            for (std::size_t pixel = 0; pixel < pixel_count; ++pixel)
            {
                append_rgba(result.m_rgba, data.data() + image_offset + pixel * 4);
            }
            return result;
        }

        if (image_type != 4 && image_type != 5)
        {
            throw std::runtime_error("unsupported TIM2 image type " + std::to_string(image_type));
        }
        if ((clut_type & 0x3F) != 3)
        {
            throw std::runtime_error("TIM2 indexed texture CLUT is not RGBA32");
        }
        if (*clut_size < static_cast<std::uint32_t>(*colour_count) * 4u)
        {
            throw std::runtime_error("TIM2 palette is smaller than its colour count");
        }

        const std::size_t expected_image_size = image_type == 4
            ? (pixel_count + 1) / 2
            : pixel_count;
        if (*image_size != expected_image_size)
        {
            throw std::runtime_error("unexpected TIM2 indexed image size");
        }

        result.m_format = image_type == 4 ? e_texture_format::indexed4 : e_texture_format::indexed8;
        const std::size_t colour_count_size = *colour_count;
        for (std::size_t pixel = 0; pixel < pixel_count; ++pixel)
        {
            std::uint8_t colour_index = 0;
            if (image_type == 4)
            {
                const std::uint8_t packed = data[image_offset + pixel / 2];
                colour_index = static_cast<std::uint8_t>((packed >> ((pixel & 1u) * 4u)) & 0x0Fu);
            }
            else
            {
                colour_index = data[image_offset + pixel];
                // GS CSM1 palette index permutation used by the indexed8 samples.
                if ((clut_type & 0x80u) == 0)
                {
                    colour_index = static_cast<std::uint8_t>((colour_index & ~0x18u) |
                        ((colour_index & 0x08u) << 1) | ((colour_index & 0x10u) >> 1));
                }
            }

            if (colour_index >= colour_count_size)
            {
                throw std::runtime_error("TIM2 palette index is out of bounds");
            }
            append_rgba(result.m_rgba, data.data() + palette_offset + static_cast<std::size_t>(colour_index) * 4);
        }

        return result;
    }

    s_texture_image decode_xpix(const std::span<const std::uint8_t> data)
    {
        if (data.size() >= 4 && fourcc_to_string(data.data()) == "TIM2")
        {
            return decode_tim2(data);
        }
        if (data.size() < 36 || fourcc_to_string(data.data()) != "XPIX")
        {
            throw std::runtime_error("expected XPIX wrapper or TIM2 texture");
        }
        if (fourcc_to_string(data.data() + 32) != "TIM2")
        {
            throw std::runtime_error("XPIX wrapper does not contain a TIM2 payload at +0x20");
        }
        return decode_tim2(data.subspan(32));
    }

    s_texture_image decode_first_fpix_texture(const std::span<const std::uint8_t> data)
    {
        const auto container = parse_chunked_container(data, "FPIX");
        for (const auto& chunk : container.m_chunks)
        {
            if (chunk.magic_string() == "XPIX")
            {
                return decode_xpix(data.subspan(chunk.m_offset, chunk.m_size));
            }
        }
        throw std::runtime_error("FPIX container does not contain an XPIX texture chunk");
    }

    void write_bmp_rgba(const std::filesystem::path& path, const s_texture_image& image)
    {
        if (image.m_width == 0 || image.m_height == 0 ||
            image.m_rgba.size() != static_cast<std::size_t>(image.m_width) * image.m_height * 4)
        {
            throw std::runtime_error("invalid RGBA texture supplied for BMP export");
        }

        const std::uint64_t pixel_bytes64 = static_cast<std::uint64_t>(image.m_width) * image.m_height * 4;
        if (pixel_bytes64 > (std::numeric_limits<std::uint32_t>::max)() - 54u)
        {
            throw std::runtime_error("texture is too large for a BMP file");
        }

        const std::uint32_t pixel_bytes = static_cast<std::uint32_t>(pixel_bytes64);
        std::vector<std::uint8_t> output(54u + pixel_bytes, 0);
        output[0] = 'B';
        output[1] = 'M';
        write_u32_le(output, 2, static_cast<std::uint32_t>(output.size()));
        write_u32_le(output, 10, 54);
        write_u32_le(output, 14, 40);
        write_u32_le(output, 18, image.m_width);
        write_u32_le(output, 22, image.m_height);
        write_u16_le(output, 26, 1);
        write_u16_le(output, 28, 32);
        write_u32_le(output, 34, pixel_bytes);

        const std::size_t row_size = static_cast<std::size_t>(image.m_width) * 4;
        for (std::uint32_t y = 0; y < image.m_height; ++y)
        {
            const std::size_t source_row = static_cast<std::size_t>(y) * row_size;
            const std::size_t destination_row = 54 + static_cast<std::size_t>(image.m_height - 1 - y) * row_size;
            for (std::uint32_t x = 0; x < image.m_width; ++x)
            {
                const std::size_t source = source_row + static_cast<std::size_t>(x) * 4;
                const std::size_t destination = destination_row + static_cast<std::size_t>(x) * 4;
                output[destination + 0] = image.m_rgba[source + 2];
                output[destination + 1] = image.m_rgba[source + 1];
                output[destination + 2] = image.m_rgba[source + 0];
                output[destination + 3] = image.m_rgba[source + 3];
            }
        }

        write_file(path, output);
    }
}
