#include "core/original_spawn.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace
{
    int g_failures = 0;

    void require(const bool condition, const char* message)
    {
        if (!condition)
        {
            ++g_failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    bool floats_equal(float a, float b)
    {
        return std::abs(a - b) < 1.0e-4f;
    }

    bool descriptor_eq(const sdbz::s_original_spawn_descriptor& d,
                       float v0, float v1, float v2, float v3)
    {
        return floats_equal(d.m_value[0], v0) &&
               floats_equal(d.m_value[1], v1) &&
               floats_equal(d.m_value[2], v2) &&
               floats_equal(d.m_value[3], v3);
    }
}  // namespace

int main()
{
    // --- Map index arithmetic: field_08 * 2 + field_24 ---
    require(sdbz::original_spawn_map_index(0u, 0u) == 0u, "map index 0,0 -> 0");
    require(sdbz::original_spawn_map_index(1u, 0u) == 2u, "map index 1,0 -> 2");
    require(sdbz::original_spawn_map_index(1u, 1u) == 3u, "map index 1,1 -> 3");
    require(sdbz::original_spawn_map_index(12u, 1u) == 25u, "map index 12,1 -> 25");
    require(sdbz::original_spawn_map_index(0u, 5u) == 5u, "map index 0,5 -> 5");

    // --- Confirmed group lookups from the observed map (ELF 0x00488400) ---
    require(sdbz::original_spawn_group_for_stage(0u, 0u) == 0u, "group 0");
    require(sdbz::original_spawn_group_for_stage(1u, 0u) == 1u, "group 1");
    require(sdbz::original_spawn_group_for_stage(3u, 1u) == 4u, "group 4");
    require(sdbz::original_spawn_group_for_stage(12u, 0u) == 15u, "group 15");
    require(sdbz::original_spawn_group_for_stage(12u, 1u) == 15u, "group 15b");
    require(!sdbz::original_spawn_group_for_stage(13u, 0u).has_value(),
        "index 26 is outside the observed map");

    // --- Record offset math: group * 0x50 + slot * 0x10 ---
    require(sdbz::original_spawn_record_offset(0u, 0u) == 0u, "offset 0,0 -> 0");
    require(sdbz::original_spawn_record_offset(0u, 1u) == 16u, "offset 0,1 -> 16");
    require(sdbz::original_spawn_record_offset(1u, 0u) == 0x50u, "offset 1,0 -> 80");
    require(sdbz::original_spawn_record_offset(15u, 4u) == 1264u,
        "offset 15,4 -> 1264 (last record start)");

    // --- Embedded table spans 320 floats ---
    const auto table = sdbz::original_spawn_table_span();
    require(table.size() == sdbz::k_original_spawn_table_float_count,
        "embedded table spans all 320 floats");

    // --- Descriptor reads return the real extracted bytes ---
    {
        const auto d = sdbz::original_spawn_read_descriptor(1u, 0u);
        require(d.has_value() && descriptor_eq(*d, 20.3f, 66.0f, 14.5f, 34.4f),
            "group 1 slot 0 matches extracted ELF bytes");
    }
    {
        const auto d = sdbz::original_spawn_read_descriptor(1u, 1u);
        require(d.has_value() && descriptor_eq(*d, -175.5f, 204.7f, -201.1f, 208.0f),
            "group 1 slot 1 matches extracted ELF bytes");
    }
    {
        const auto d = sdbz::original_spawn_read_descriptor(15u, 0u);
        require(d.has_value() && descriptor_eq(*d, -15.0f, 0.1f, 15.0f, 0.1f),
            "group 15 slot 0 matches extracted ELF bytes");
    }
    {
        const auto d = sdbz::original_spawn_read_descriptor(13u, 1u);
        require(d.has_value() && descriptor_eq(*d, -165.0f, 0.0f, -135.0f, 0.0f),
            "group 13 slot 1 matches extracted ELF bytes");
    }
    {
        const auto d = sdbz::original_spawn_read_descriptor(13u, 2u);
        require(d.has_value() && descriptor_eq(*d, 135.0f, 0.0f, 165.0f, 0.0f),
            "group 13 slot 2 matches extracted ELF bytes");
    }
    {
        const auto d = sdbz::original_spawn_read_descriptor(0u, 0u);
        require(d.has_value() && descriptor_eq(*d, -15.0f, 0.0f, 15.0f, 0.0f),
            "group 0 slot 0 is the sentinel {-15, 0, 15, 0}");
    }

    // --- Span-based read against the embedded table ---
    {
        std::span<const float> span = table;
        const auto d = sdbz::original_spawn_read_descriptor(span, 7u, 2u);
        require(d.has_value() && descriptor_eq(*d, 279.9f, -50.8f, 272.9f, -80.5f),
            "span-based read group 7 slot 2 matches extracted ELF bytes");
    }

    // --- Bounds checks ---
    require(!sdbz::original_spawn_read_descriptor(16u, 0u).has_value(),
        "group 16 out of range");
    require(!sdbz::original_spawn_read_descriptor(0u, 5u).has_value(),
        "slot 5 out of range");
    std::vector<float> short_table(2);
    require(!sdbz::original_spawn_read_descriptor(
                short_table, 0u, 0u).has_value(),
        "undersized table yields nullopt");

    // --- Resolve transform produces correct candidate positions ---
    {
        sdbz::s_original_spawn_descriptor desc;
        desc.m_value = { 20.3f, 66.0f, 14.5f, 34.4f };
        const float dx = 14.5f - 20.3f;
        const float dz = 34.4f - 66.0f;
        const float mag = std::sqrt(dx * dx + dz * dz);
        const float nx = dx / mag;
        const float nz = dz / mag;
        const float mid_x = (20.3f + 14.5f) * 0.5f;

        // Side 0 -> Candidate A
        const auto ca = sdbz::original_resolve_player_position(desc, 0u);
        require(ca.has_value(), "resolve side 0 returns candidate A");
        if (ca)
        {
            require(std::abs((*ca)[0] - (mid_x + 15.5f * nx)) < 1.0e-3f,
                "candidate A x");
            require(std::abs((*ca)[1] - nx) < 1.0e-3f, "candidate A y");
            require(std::abs((*ca)[2] - (66.0f + 15.5f * nz)) < 1.0e-3f,
                "candidate A z");
        }

        // Side 1 -> Candidate B
        const auto cb = sdbz::original_resolve_player_position(desc, 1u);
        require(cb.has_value(), "resolve side 1 returns candidate B");
        if (cb)
        {
            require(std::abs((*cb)[0] - (mid_x - 14.5f * nx)) < 1.0e-3f,
                "candidate B x");
            require(std::abs((*cb)[1] - nx) < 1.0e-3f, "candidate B y");
            require(std::abs((*cb)[2] - (66.0f - 14.5f * nz)) < 1.0e-3f,
                "candidate B z");
        }
    }

    // --- Degenerate descriptor (dx=dz=0) returns nullopt ---
    {
        sdbz::s_original_spawn_descriptor desc;
        desc.m_value = { 5.0f, 5.0f, 5.0f, 5.0f };
        require(!sdbz::original_resolve_player_position(desc, 0u).has_value(),
            "degenerate descriptor (zero direction) returns nullopt");
    }

    if (g_failures != 0)
    {
        std::cerr << g_failures << " original-spawn test(s) failed\n";
        return 1;
    }
    std::cout << "Original-spawn tests passed\n";
    return 0;
}
