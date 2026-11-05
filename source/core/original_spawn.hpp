#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace sdbz
{

// Reference reconstruction of the original PS2 player placement routine at
// ELF virtual address 0x0023AF30 (SLUS_214(2).42). See
// SLUS_spawn_research_log.md.
//
// Status: structural port plan, NOT ready to merge into gameplay. The
// descriptor transform (helper 0x00106950), the native-stage field mapping,
// the stage vtable method at +0x28, and the fighter-side field at
// +0x534 -> +0x08 are all unresolved. The resolve functions below
// intentionally return std::nullopt until those are confirmed; do not wire
// computed positions into gameplay from this module.
//
// The confirmed structure (implementation consequence items 1-3 of the log):
//
//   1. stage map index  = field_08 * 2 + field_24            -> 24-entry map
//   2. map entry        -> placement group (0-15)
//   3. stage vtable +0x28 -> slot selector (0-4)
//   4. record offset    = group * 0x50 + slot * 0x10          -> 4 floats
//
// Items 4-6 (descriptor transform, fighter-side selection, position setter)
// remain open and are deliberately unimplemented here.

// ELF virtual address of the 24-entry stage-group map. It immediately
// follows the placement table at 0x00487E00 (16 groups x 5 slots x 4 floats
// = 0x500 bytes).
constexpr std::uint32_t k_original_spawn_map_address = 0x00488400u;

// ELF virtual address of the placement table.
constexpr std::uint32_t k_original_spawn_table_address = 0x00487E00u;

constexpr std::size_t k_original_spawn_group_count = 16;
constexpr std::size_t k_original_spawn_slot_count = 5;
constexpr std::size_t k_original_spawn_float_per_slot = 4;
constexpr std::size_t k_original_spawn_slot_stride_bytes = 16;  // 4 floats
constexpr std::size_t k_original_spawn_group_stride_bytes = 0x50;  // 80 bytes
constexpr std::size_t k_original_spawn_table_float_count =
    k_original_spawn_group_count * k_original_spawn_slot_count *
    k_original_spawn_float_per_slot;

// Verified placement table extracted from SLUS_214.42 at ELF virtual address
// 0x00487E00. Laid out as 16 groups x 5 slots x 4 floats (320 floats,
// 0x500 bytes). Each group selects via the stage-group map; each slot is
// selected by the stage vtable method at vtable byte offset +0x28. The four
// floats form one placement descriptor whose exact meaning and transform are
// unresolved (helper 0x00106950 uses COP2/MIPS-3D vector instructions not yet
// decoded). These are the actual bytes from the executable, not guessed data.
extern const std::array<float, 4> k_original_spawn_placement_table
    [k_original_spawn_group_count][k_original_spawn_slot_count];

// Observed stage-group map entries (ELF 0x00488400). Indices 0-23 are the
// confirmed meaningful entries selecting one of 16 placement groups. Indices
// 24-25 (values 15, 15) are observed in the disassembly table but their
// validity is uncertain per the research log; indices 26+ may be adjacent
// unrelated data and are intentionally not included here.
const std::array<std::uint32_t, 26>& original_spawn_stage_group_map() noexcept;

// Stage-key map index = field_08 * 2 + field_24. Pure arithmetic: the caller
// is responsible for bounds-checking the result against the map.
[[nodiscard]] constexpr std::size_t original_spawn_map_index(
    std::uint32_t field_08, std::uint32_t field_24) noexcept
{
    return static_cast<std::size_t>(field_08) * 2u +
           static_cast<std::size_t>(field_24);
}

// Bounds-checked group lookup for a map index. Returns nullopt when the index
// falls outside the observed map.
[[nodiscard]] inline std::optional<std::uint32_t> original_spawn_lookup_group(
    std::size_t map_index) noexcept
{
    if (map_index >= original_spawn_stage_group_map().size())
    {
        return std::nullopt;
    }
    return original_spawn_stage_group_map()[map_index];
}

// Convenience: derive the placement group directly from the two stage key
// fields. Returns nullopt when the combined index is out of map range.
[[nodiscard]] inline std::optional<std::uint32_t>
original_spawn_group_for_stage(std::uint32_t field_08,
                               std::uint32_t field_24) noexcept
{
    return original_spawn_lookup_group(
        original_spawn_map_index(field_08, field_24));
}

// Byte offset of the selected slot's four-float record within the placement
// table: group_index * 0x50 + slot_index * 0x10.
[[nodiscard]] constexpr std::size_t original_spawn_record_offset(
    std::size_t group_index, std::size_t slot_index) noexcept
{
    return group_index * k_original_spawn_group_stride_bytes +
           slot_index * k_original_spawn_slot_stride_bytes;
}

// One selected slot's four-float placement descriptor from the table.
struct s_original_spawn_descriptor
{
    std::array<float, 4> m_value{};
};

// Bounds-checked read of a table descriptor. Returns nullopt when the group
// or slot is out of range, or when the table span is too short to contain the
// record. Pure in the table data: no ELF file access is performed here.
[[nodiscard]] inline std::optional<s_original_spawn_descriptor>
original_spawn_read_descriptor(std::span<const float> table,
                               std::size_t group_index,
                               std::size_t slot_index) noexcept
{
    if (group_index >= k_original_spawn_group_count ||
        slot_index >= k_original_spawn_slot_count)
    {
        return std::nullopt;
    }
    const std::size_t first_float =
        group_index * k_original_spawn_slot_count * k_original_spawn_float_per_slot +
        slot_index * k_original_spawn_float_per_slot;
    if (table.size() < first_float + k_original_spawn_float_per_slot)
    {
        return std::nullopt;
    }
    s_original_spawn_descriptor descriptor;
    descriptor.m_value[0] = table[first_float];
    descriptor.m_value[1] = table[first_float + 1];
    descriptor.m_value[2] = table[first_float + 2];
    descriptor.m_value[3] = table[first_float + 3];
    return descriptor;
}

// Read a descriptor from the embedded verified table (ELF 0x00487E00).
// Returns nullopt when the group or slot is out of range.
[[nodiscard]] std::optional<s_original_spawn_descriptor>
original_spawn_read_descriptor(std::size_t group_index,
                               std::size_t slot_index) noexcept;

// Flat span over the embedded verified table (320 floats).
[[nodiscard]] std::span<const float> original_spawn_table_span() noexcept;

// Helper 0x00106950: vector normalization. Takes a 4-float vector at *a0,
// normalizes the xz components to unit length (y=0 assumed, w ignored).
// Net: nx = dx/sqrt(dx*dx+dz*dz), nz = dz/sqrt(dx*dx+dz*dz).
// COP2/MIPS-3D: vmul.xyz (square xz), vaddy.x (accumulate x²), vaddz.x
// (accumulate z²), vrsqrt Q (Q=1/sqrt(sum)), vwaitq, vmulq.xyz (scale).
// This is NOT a matrix/rotation transform — it is a 2D (xz-plane) normalization.

// Field semantics (high confidence from caller disassembly):
// - stage+0x08: stage ID (0-12 for 13 stages)
// - stage+0x24: variant/sub-stage bit (0/1)
// - stage vtable +0x28: spawn-slot selector (0-4)
// - player+0x534->+0x08: side/player-index selector (P1 vs P2)
// - player vtable +0x40: world-space 3-float position setter
// - Coordinate convention: [x, y, z, w], Y-up, ground at y=0, w~1.0 padding

// Full descriptor-to-candidate transform (RESOLVED from decoded disassembly):
//   dx = table[2] - table[0]
//   dz = table[3] - table[1]
//   mag = sqrt(dx*dx + dz*dz)
//   nx = dx / mag
//   nz = dz / mag
//   mid_x = (table[0] + table[2]) / 2.0f
//   mid_z = (table[1] + table[3]) / 2.0f   (not used directly)
//   Candidate A (sp+0x150): { mid_x + 15.5*nx, nx, table[1] + 15.5*nz, 1.0 }
//   Candidate B (sp+0x160): { mid_x - 14.5*nx, nx, table[1] - 14.5*nz, 1.0 }
//   Side selector: player->0x534->0x08 == 1 -> Candidate B, == 0 -> Candidate A.

// Stage-to-placement-group mapping (field_08 = stage ID 0-12, field_24 = variant 0/1).
// map_index = stage_id * 2 + variant. Returns the placement group (0-15).
// Sentinel groups (0,3,4,9,10,15) have no real placement data.
[[nodiscard]] inline std::optional<std::uint32_t>
original_spawn_group_for_stage_id(std::uint32_t stage_id, std::uint32_t variant) noexcept
{
    return original_spawn_group_for_stage(stage_id, variant);
}
[[nodiscard]] inline std::optional<std::array<float, 3>>
original_resolve_player_position(const s_original_spawn_descriptor& descriptor,
                                 std::uint32_t fighter_side_field) noexcept
{
    const float dx = descriptor.m_value[2] - descriptor.m_value[0];
    const float dz = descriptor.m_value[3] - descriptor.m_value[1];
    const float mag = std::sqrtf(dx * dx + dz * dz);
    if (mag <= 0.0f)
    {
        return std::nullopt;
    }
    const float nx = dx / mag;
    const float nz = dz / mag;
    const float mid_x = (descriptor.m_value[0] + descriptor.m_value[2]) * 0.5f;

    std::array<float, 3> candidate;
    if (fighter_side_field == 1u)
    {
        // Candidate B (sp+0x160)
        candidate[0] = mid_x - 14.5f * nx;
        candidate[1] = nx;
        candidate[2] = descriptor.m_value[1] - 14.5f * nz;
    }
    else
    {
        // Candidate A (sp+0x150)
        candidate[0] = mid_x + 15.5f * nx;
        candidate[1] = nx;
        candidate[2] = descriptor.m_value[1] + 15.5f * nz;
    }
    return candidate;
}

}  // namespace sdbz
