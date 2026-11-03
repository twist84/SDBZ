#include "core/container.hpp"

#include "core/binary_reader.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sdbz
{
    std::string s_container_chunk::magic_string() const
    {
        return std::string(m_magic.begin(), m_magic.end());
    }

    s_chunked_container parse_chunked_container(
        const std::span<const std::uint8_t> data,
        std::string_view expected_magic)
    {
        if (data.size() < 32)
        {
            throw std::runtime_error("container is smaller than its 0x20-byte header");
        }

        s_chunked_container result;
        std::copy_n(data.begin(), result.m_magic.size(), result.m_magic.begin());
        const std::string actual_magic(result.m_magic.begin(), result.m_magic.end());
        if (!expected_magic.empty() && actual_magic != expected_magic)
        {
            throw std::runtime_error(std::string("expected ") + std::string(expected_magic) + " container, found " + actual_magic);
        }
        if (expected_magic.empty() && actual_magic != "FPIX" && actual_magic != "FMDL")
        {
            throw std::runtime_error("unsupported top-level container magic: " + actual_magic);
        }

        const auto file_size = read_u32_le(data, 4);
        const auto chunk_count = read_u32_le(data, 8);
        const auto format = read_u32_le(data, 12);
        if (!file_size || !chunk_count || !format)
        {
            throw std::runtime_error("container header is truncated");
        }
        if (*file_size != data.size())
        {
            throw std::runtime_error("container size field does not match the supplied payload size");
        }
        if (*chunk_count == 0)
        {
            throw std::runtime_error("container has no chunks");
        }

        const std::uint64_t table_end64 = 32ull + static_cast<std::uint64_t>(*chunk_count) * 4ull;
        if (table_end64 > data.size())
        {
            throw std::runtime_error("container offset table exceeds file size");
        }

        const std::size_t table_end = static_cast<std::size_t>(table_end64);
        result.m_file_size = *file_size;
        result.m_chunk_count = *chunk_count;
        result.m_format = *format;
        result.m_chunks.reserve(*chunk_count);

        std::vector<std::uint32_t> offsets;
        offsets.reserve(*chunk_count);
        for (std::size_t index = 0; index < *chunk_count; ++index)
        {
            const auto offset = read_u32_le(data, 32 + index * 4);
            if (!offset)
            {
                throw std::runtime_error("container offset table is truncated");
            }
            if (*offset < table_end || *offset >= data.size())
            {
                throw std::runtime_error("container chunk offset is outside the chunk-data region");
            }
            if (!offsets.empty() && *offset <= offsets.back())
            {
                throw std::runtime_error("container chunk offsets are not strictly increasing");
            }
            offsets.push_back(*offset);
        }

        if (offsets.front() != table_end)
        {
            throw std::runtime_error("first container chunk does not immediately follow its offset table");
        }

        for (std::size_t index = 0; index < offsets.size(); ++index)
        {
            const std::uint32_t start = offsets[index];
            const std::uint32_t end = index + 1 < offsets.size()
                ? offsets[index + 1]
                : *file_size;
            if (end <= start || end - start < 8)
            {
                throw std::runtime_error("container chunk has an invalid extent");
            }

            const auto chunk_size = read_u32_le(data, static_cast<std::size_t>(start) + 4);
            if (!chunk_size || *chunk_size != end - start)
            {
                throw std::runtime_error("container chunk size does not match its directory extent");
            }

            s_container_chunk chunk;
            std::copy_n(data.begin() + start, chunk.m_magic.size(), chunk.m_magic.begin());
            chunk.m_offset = start;
            chunk.m_size = *chunk_size;
            result.m_chunks.push_back(chunk);
        }

        return result;
    }
}
