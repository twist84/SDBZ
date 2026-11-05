#include "core/original_spawn.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace sdbz
{

// Verified stage-group map entries at ELF virtual address 0x00488400, one u32
// per entry, little-endian. Indices 0-23 are the confirmed meaningful entries
// selecting one of 16 placement groups (0-15). Indices 24-25 (values 15, 15)
// are observed in the disassembly table but their validity is uncertain per
// SLUS_spawn_research_log.md; indices 26+ may be adjacent unrelated data and
// are intentionally not included here.
//
// Extracted from assets/original/SLUS_214.42 (file offset 0x388480).
const std::array<std::uint32_t, 26> k_stage_group_map = {
    0u,  0u,  1u,  1u,  2u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,
    9u,  9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u, 14u, 14u,
    15u, 15u};

// Verified placement table at ELF virtual address 0x00487E00 (file offset
// 0x387e80). Laid out as 16 groups x 5 slots x 4 floats. Each entry is the
// exact little-endian float32 bytes from the executable.
//
// Slot patterns observed:
//   - Groups 0, 3, 4, 9, 10: all slots are the sentinel {-15, 0, 15, 0}.
//   - Group 15: all slots are {-15, 0.1, 15, 0.1}.
//   - Group 13: slot 0 sentinel, slot 1 {-165, 0, -135, 0}, slot 2 {135, 0,
//     165, 0}, slots 3-4 sentinel.
//   - Remaining groups carry distinct per-slot descriptors.
const std::array<float, 4>
    k_original_spawn_placement_table[k_original_spawn_group_count]
        [k_original_spawn_slot_count] = {
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
        },
        {
            { 20.3f, 66.0f, 14.5f, 34.4f },
            { -175.5f, 204.7f, -201.1f, 208.0f },
            { -182.6f, -80.4f, -141.7f, -91.9f },
            { -175.5f, 204.7f, -201.1f, 208.0f },
            { -182.6f, -80.4f, -141.7f, -91.9f },
        },
        {
            { -85.0f, 70.0f, -85.0f, 40.0f },
            { -335.9f, -194.0f, -307.3f, -182.3f },
            { -67.3f, -183.7f, -37.1f, -185.7f },
            { 432.4f, 44.2f, 451.5f, 34.2f },
            { -335.9f, -194.0f, -307.3f, -182.3f },
        },
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
        },
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
        },
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { 104.7f, 234.4f, 82.7f, 260.6f },
            { 174.5f, -41.8f, 204.4f, -36.8f },
            { -302.0f, -123.7f, -297.7f, -90.8f },
            { 104.7f, 234.4f, 82.7f, 260.6f },
        },
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -71.5f, 347.8f, -79.3f, 323.8f },
            { 303.1f, -114.1f, 311.1f, -145.8f },
            { -278.9f, 30.1f, -304.0f, 9.1f },
            { -71.5f, 347.8f, -79.3f, 323.8f },
        },
        {
            { -3.2f, 6.9f, 22.4f, -3.4f },
            { -113.7f, 242.8f, -96.9f, 222.7f },
            { 279.9f, -50.8f, 272.9f, -80.5f },
            { -113.7f, 242.8f, -96.9f, 222.7f },
            { 278.3f, -79.5f, 265.0f, -105.6f },
        },
        {
            { -18.9f, 42.6f, 10.4f, 36.4f },
            { -127.2f, 276.5f, -110.4f, 249.4f },
            { 168.0f, -35.4f, 182.9f, -62.0f },
            { -127.2f, 276.5f, -110.4f, 249.4f },
            { 168.0f, -35.4f, 182.9f, -62.0f },
        },
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
        },
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
        },
        {
            { 20.3f, 66.0f, 14.5f, 34.4f },
            { -175.5f, 204.7f, -201.1f, 208.0f },
            { -182.6f, -80.4f, -141.7f, -91.9f },
            { -175.5f, 204.7f, -201.1f, 208.0f },
            { -182.6f, -80.4f, -141.7f, -91.9f },
        },
        {
            { -85.0f, 70.0f, -85.0f, 40.0f },
            { -335.9f, -194.0f, -307.3f, -182.3f },
            { -67.3f, -183.7f, -37.1f, -185.7f },
            { 432.4f, 44.2f, 451.5f, 34.2f },
            { -335.9f, -194.0f, -307.3f, -182.3f },
        },
        {
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -165.0f, 0.0f, -135.0f, 0.0f },
            { 135.0f, 0.0f, 165.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
            { -15.0f, 0.0f, 15.0f, 0.0f },
        },
        {
            { 20.3f, 66.0f, 14.5f, 34.4f },
            { -175.5f, 204.7f, -201.1f, 208.0f },
            { -182.6f, -80.4f, -141.7f, -91.9f },
            { -175.5f, 204.7f, -201.1f, 208.0f },
            { -182.6f, -80.4f, -141.7f, -91.9f },
        },
        {
            { -15.0f, 0.1f, 15.0f, 0.1f },
            { -15.0f, 0.1f, 15.0f, 0.1f },
            { -15.0f, 0.1f, 15.0f, 0.1f },
            { -15.0f, 0.1f, 15.0f, 0.1f },
            { -15.0f, 0.1f, 15.0f, 0.1f },
        },
    };

const std::array<std::uint32_t, 26>&
original_spawn_stage_group_map() noexcept
{
    return k_stage_group_map;
}

std::optional<s_original_spawn_descriptor>
original_spawn_read_descriptor(std::size_t group_index,
                               std::size_t slot_index) noexcept
{
    if (group_index >= k_original_spawn_group_count ||
        slot_index >= k_original_spawn_slot_count)
    {
        return std::nullopt;
    }
    s_original_spawn_descriptor descriptor;
    descriptor.m_value[0] = k_original_spawn_placement_table[group_index]
                                               [slot_index][0];
    descriptor.m_value[1] = k_original_spawn_placement_table[group_index]
                                               [slot_index][1];
    descriptor.m_value[2] = k_original_spawn_placement_table[group_index]
                                               [slot_index][2];
    descriptor.m_value[3] = k_original_spawn_placement_table[group_index]
                                               [slot_index][3];
    return descriptor;
}

std::span<const float> original_spawn_table_span() noexcept
{
    return { reinterpret_cast<const float*>(
                 k_original_spawn_placement_table),
             k_original_spawn_table_float_count };
}

}  // namespace sdbz
