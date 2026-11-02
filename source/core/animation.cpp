#include "core/animation.hpp"

#include "core/binary_reader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sdbz
{
    namespace
    {
        struct s_channel_array
        {
            std::uint16_t m_channel_count = 0;
            std::uint16_t m_key_count = 0;
            std::vector<float> m_times;
            std::vector<std::array<float, 4>> m_values;
        };

        [[nodiscard]] std::array<float, 4> read_vec4(
            const std::span<const std::uint8_t> data,
            const std::size_t offset)
        {
            std::array<float, 4> result{};
            for (std::size_t component = 0; component < result.size(); ++component)
            {
                const auto value = read_f32_le(data, offset + component * 4);
                if (!value || !std::isfinite(*value))
                {
                    throw std::runtime_error("MOT2 contains a truncated or non-finite float vector");
                }
                result[component] = *value;
            }
            return result;
        }

        [[nodiscard]] std::vector<std::array<float, 4>> read_vectors(
            const std::span<const std::uint8_t> data,
            const std::size_t offset,
            const std::size_t count)
        {
            if (count > ((std::numeric_limits<std::size_t>::max)() / 16) ||
                !has_range(data.size(), offset, count * 16))
            {
                throw std::runtime_error("MOT2 float-vector pool exceeds the file extent");
            }
            std::vector<std::array<float, 4>> result;
            result.reserve(count);
            for (std::size_t index = 0; index < count; ++index)
            {
                result.push_back(read_vec4(data, offset + index * 16));
            }
            return result;
        }

        [[nodiscard]] std::string fixed_string(
            const std::span<const std::uint8_t> data,
            const std::size_t offset,
            const std::size_t length)
        {
            if (!has_range(data.size(), offset, length))
            {
                throw std::runtime_error("RFSM fixed-size name exceeds the file extent");
            }
            std::size_t end = offset;
            while (end < offset + length && data[end] != 0)
            {
                ++end;
            }
            return std::string(
                reinterpret_cast<const char*>(data.data() + offset),
                end - offset);
        }

        [[nodiscard]] std::size_t checked_count_product(
            const std::size_t left,
            const std::size_t right,
            const std::string_view description)
        {
            if (right != 0 && left > (std::numeric_limits<std::size_t>::max)() / right)
            {
                throw std::runtime_error(std::string(description) + " size overflow");
            }
            return left * right;
        }

        [[nodiscard]] std::array<float, 4> normalized_quaternion(const std::array<float, 4>& value)
        {
            const float length_squared = value[0] * value[0] + value[1] * value[1] +
                value[2] * value[2] + value[3] * value[3];
            if (!(length_squared > 1.0e-12f))
            {
                throw std::runtime_error("MOT2 contains a zero-length rotation quaternion");
            }
            const float inverse_length = 1.0f / std::sqrt(length_squared);
            return
            {
                value[0] * inverse_length,
                value[1] * inverse_length,
                value[2] * inverse_length,
                value[3] * inverse_length
            };
        }

        [[nodiscard]] s_animation_track make_track(
            const std::vector<std::array<float, 4>>& constants,
            const s_channel_array& channel,
            const std::uint16_t flags,
            const std::uint16_t index,
            const std::uint16_t component_flag,
            const bool quaternion,
            std::vector<std::uint8_t>& observed)
        {
            s_animation_track result;
            result.m_is_animated = (flags & component_flag) != 0;
            if (result.m_is_animated)
            {
                if (channel.m_key_count == 0 || index % channel.m_key_count != 0)
                {
                    throw std::runtime_error("MOT2 animated track has an invalid vector index");
                }
                const std::size_t channel_index = index / channel.m_key_count;
                if (channel_index >= channel.m_channel_count)
                {
                    throw std::runtime_error("MOT2 animated track channel index is invalid");
                }
                observed[channel_index] = 1;
                result.m_channel_index = static_cast<std::uint16_t>(channel_index);
                const std::size_t start = index;
                result.m_times = channel.m_times;
                result.m_values.assign(
                    channel.m_values.begin() + static_cast<std::ptrdiff_t>(start),
                    channel.m_values.begin() + static_cast<std::ptrdiff_t>(start + channel.m_key_count));
            }
            else
            {
                if (index >= constants.size())
                {
                    throw std::runtime_error("MOT2 constant track references a missing float4");
                }
                result.m_times.push_back(0.0f);
                result.m_values.push_back(constants[index]);
            }

            if (quaternion)
            {
                for (const auto& value : result.m_values)
                {
                    const float length_squared = value[0] * value[0] + value[1] * value[1] +
                        value[2] * value[2] + value[3] * value[3];
                    if (std::abs(length_squared - 1.0f) >= 0.01f)
                    {
                        throw std::runtime_error("MOT2 rotation channel contains a non-unit quaternion");
                    }
                }
            }
            return result;
        }

        [[nodiscard]] s_channel_array parse_channel_array(
            const std::span<const std::uint8_t> data,
            const std::size_t header_offset)
        {
            const auto channels = read_u16_le(data, header_offset);
            const auto keys = read_u16_le(data, header_offset + 2);
            const auto values_offset = read_u32_le(data, header_offset + 4);
            const auto times_offset = read_u32_le(data, header_offset + 8);
            if (!channels || !keys || !values_offset || !times_offset)
            {
                throw std::runtime_error("MOT2 channel group header is truncated");
            }

            s_channel_array result;
            result.m_channel_count = *channels;
            result.m_key_count = *keys;
            if (result.m_channel_count == 0)
            {
                return result;
            }
            if (result.m_key_count == 0)
            {
                throw std::runtime_error("MOT2 animated channel group has no keys");
            }

            const std::size_t value_count = checked_count_product(
                result.m_channel_count, result.m_key_count, "MOT2 channel values");
            result.m_values = read_vectors(data, *values_offset, value_count);
            if (!has_range(data.size(), *times_offset, checked_count_product(result.m_key_count, 4, "MOT2 key times")))
            {
                throw std::runtime_error("MOT2 key-time array exceeds the file extent");
            }

            result.m_times.reserve(result.m_key_count);
            for (std::size_t key = 0; key < result.m_key_count; ++key)
            {
                const auto time = read_f32_le(data, *times_offset + key * 4);
                if (!time || !std::isfinite(*time) || *time < 0.0f ||
                    (!result.m_times.empty() && *time <= result.m_times.back()))
                {
                    throw std::runtime_error("MOT2 key times are invalid or not strictly increasing");
                }
                result.m_times.push_back(*time);
            }
            return result;
        }
    }

    s_animation_clip parse_mot2(const std::span<const std::uint8_t> data, const std::string& name)
    {
        if (data.size() < 64 || fourcc_to_string(data.data()) != "MOT2")
        {
            throw std::runtime_error("expected MOT2 animation data");
        }
        const auto declared_size = read_u32_le(data, 4);
        const auto fps = read_f32_le(data, 8);
        const auto frame_count = read_u16_le(data, 12);
        const auto node_count = read_u16_le(data, 14);
        const auto constant_count = read_u16_le(data, 16);
        const auto channel_count = read_u16_le(data, 18);
        const auto node_table_offset = read_u32_le(data, 20);
        const auto constant_pool_offset = read_u32_le(data, 24);
        if (!declared_size || *declared_size != data.size() || !fps || !std::isfinite(*fps) || *fps <= 0.0f ||
            !frame_count || *frame_count == 0 || !node_count || !constant_count || !channel_count ||
            !node_table_offset || !constant_pool_offset)
        {
            throw std::runtime_error("MOT2 header fields are invalid");
        }

        const std::size_t constants_bytes = checked_count_product(*constant_count, 16, "MOT2 constant pool");
        if (*constant_pool_offset > data.size() ||
            constants_bytes > data.size() - *constant_pool_offset ||
            *constant_pool_offset + constants_bytes != *node_table_offset)
        {
            throw std::runtime_error("MOT2 constant pool and node table are inconsistent");
        }
        const std::size_t node_table_bytes = checked_count_product(*node_count, 8, "MOT2 node table");
        if (!has_range(data.size(), *node_table_offset, node_table_bytes))
        {
            throw std::runtime_error("MOT2 node descriptor table exceeds the file extent");
        }

        const auto constants = read_vectors(data, *constant_pool_offset, *constant_count);
        std::array<s_channel_array, 3> channels =
        {
            parse_channel_array(data, 28),
            parse_channel_array(data, 40),
            parse_channel_array(data, 52)
        };
        const std::uint32_t observed_channel_count =
            static_cast<std::uint32_t>(channels[0].m_channel_count) +
            static_cast<std::uint32_t>(channels[1].m_channel_count) +
            static_cast<std::uint32_t>(channels[2].m_channel_count);
        if (observed_channel_count != *channel_count)
        {
            throw std::runtime_error("MOT2 channel count does not match its three channel groups");
        }

        s_animation_clip result;
        result.m_name = name;
        result.m_frames_per_second = *fps;
        result.m_frame_count = *frame_count;
        result.m_duration = static_cast<float>(*frame_count) / *fps;
        result.m_last_frame_time = static_cast<float>(*frame_count - 1) / *fps;
        for (std::size_t type = 0; type < channels.size(); ++type)
        {
            result.m_channel_counts[type] = channels[type].m_channel_count;
        }

        std::array<std::vector<std::uint8_t>, 3> observed;
        for (std::size_t type = 0; type < channels.size(); ++type)
        {
            observed[type].resize(channels[type].m_channel_count, 0);
        }

        result.m_nodes.reserve(*node_count);
        for (std::size_t node_index = 0; node_index < *node_count; ++node_index)
        {
            const std::size_t offset = *node_table_offset + node_index * 8;
            const auto flags = read_u16_le(data, offset);
            const auto translation_index = read_u16_le(data, offset + 2);
            const auto rotation_index = read_u16_le(data, offset + 4);
            const auto scale_index = read_u16_le(data, offset + 6);
            if (!flags || !translation_index || !rotation_index || !scale_index || *flags > 7)
            {
                throw std::runtime_error("MOT2 node descriptor is invalid");
            }

            s_animation_node node;
            node.m_flags = *flags;
            node.m_translation = make_track(constants, channels[0], *flags, *translation_index,
                1, false, observed[0]);
            node.m_rotation = make_track(constants, channels[1], *flags, *rotation_index,
                2, true, observed[1]);
            node.m_scale = make_track(constants, channels[2], *flags, *scale_index,
                4, false, observed[2]);
            result.m_nodes.push_back(std::move(node));
        }

        for (std::size_t type = 0; type < channels.size(); ++type)
        {
            if (std::find(observed[type].begin(), observed[type].end(), 0) != observed[type].end())
            {
                throw std::runtime_error("MOT2 contains an animated channel not referenced by any node");
            }
        }

        return result;
    }

    s_motion_package parse_rfsm(const std::span<const std::uint8_t> data)
    {
        if (data.size() < 16 || fourcc_to_string(data.data()) != "RFSM" ||
            data[4] != 'L' || data[5] != '3')
        {
            throw std::runtime_error("expected RFSM/L3 motion package");
        }
        const auto motion_count = read_u16_le(data, 6);
        const auto shared_offset = read_u32_le(data, 8);
        const auto shared_end = read_u32_le(data, 12);
        if (!motion_count || !shared_offset || !shared_end ||
            *shared_offset != 16 + static_cast<std::uint32_t>(*motion_count) * 32u ||
            *shared_end < *shared_offset + 16 || *shared_end > data.size())
        {
            throw std::runtime_error("RFSM motion package header is invalid");
        }

        const auto shared_count = read_u16_le(data, *shared_offset);
        const auto bone_count = read_u16_le(data, *shared_offset + 2);
        const auto names_offset = read_u32_le(data, *shared_offset + 4);
        const auto bindings_offset = read_u32_le(data, *shared_offset + 8);
        const auto flags_offset = read_u32_le(data, *shared_offset + 12);
        if (!shared_count || !bone_count || !names_offset || !bindings_offset || !flags_offset ||
            *shared_count != *motion_count || *names_offset != *shared_offset + 16 ||
            *bindings_offset != *names_offset + static_cast<std::uint32_t>(*bone_count) * 16u ||
            static_cast<std::uint64_t>(*flags_offset) + *bone_count != *shared_end ||
            static_cast<std::uint64_t>(*bindings_offset) + static_cast<std::uint64_t>(*motion_count) * 12u > *flags_offset)
        {
            throw std::runtime_error("RFSM shared skeleton block is invalid");
        }

        s_motion_package result;
        result.m_bone_names.reserve(*bone_count);
        result.m_bone_flags.reserve(*bone_count);
        for (std::size_t bone = 0; bone < *bone_count; ++bone)
        {
            result.m_bone_names.push_back(fixed_string(data, *names_offset + bone * 16, 16));
            result.m_bone_flags.push_back(data[*flags_offset + bone]);
        }

        result.m_motions.reserve(*motion_count);
        for (std::size_t motion_index = 0; motion_index < *motion_count; ++motion_index)
        {
            const std::size_t record = 16 + motion_index * 32;
            const std::string motion_name = fixed_string(data, record, 16);
            const auto offset = read_u32_le(data, record + 16);
            const auto size = read_u32_le(data, record + 20);
            const auto unknown = read_u32_le(data, record + 24);
            const auto format = read_u32_le(data, record + 28);
            (void)format;
            if (!offset || !size || !unknown || *unknown != 0 || *size < 64 ||
                !has_range(data.size(), *offset, *size) || fourcc_to_string(data.data() + *offset) != "MOT2")
            {
                throw std::runtime_error("RFSM motion record has an invalid MOT2 extent");
            }

            auto clip = parse_mot2(data.subspan(*offset, *size), motion_name);
            const std::size_t binding_record = *bindings_offset + motion_index * 12;
            const auto binding_unknown = read_u32_le(data, binding_record);
            const auto forward_offset = read_u32_le(data, binding_record + 4);
            const auto reverse_offset = read_u32_le(data, binding_record + 8);
            if (!binding_unknown || !forward_offset || !reverse_offset || *binding_unknown != 0)
            {
                throw std::runtime_error("RFSM motion binding header is invalid");
            }

            const std::size_t total_channels = static_cast<std::size_t>(clip.m_channel_counts[0]) +
                clip.m_channel_counts[1] + clip.m_channel_counts[2];
            const std::size_t forward_bytes = checked_count_product(total_channels, 2, "RFSM forward bindings");
            const std::size_t reverse_bytes = checked_count_product(*bone_count, 2, "RFSM reverse bindings");
            if (!has_range(data.size(), *forward_offset, forward_bytes) ||
                !has_range(data.size(), *reverse_offset, reverse_bytes))
            {
                throw std::runtime_error("RFSM forward or reverse binding array exceeds the file extent");
            }

            std::vector<std::uint16_t> forward;
            forward.reserve(total_channels);
            for (std::size_t channel = 0; channel < total_channels; ++channel)
            {
                const auto bone = read_u16_le(data, *forward_offset + channel * 2);
                if (!bone || *bone >= *bone_count)
                {
                    throw std::runtime_error("RFSM forward binding references an invalid bone");
                }
                forward.push_back(*bone);
            }

            std::vector<std::int16_t> reverse;
            reverse.reserve(*bone_count);
            for (std::size_t bone = 0; bone < *bone_count; ++bone)
            {
                const auto raw_node_index = read_u16_le(data, *reverse_offset + bone * 2);
                if (!raw_node_index)
                {
                    throw std::runtime_error("RFSM reverse binding array is truncated");
                }
                const std::int32_t signed_node_index = (*raw_node_index & 0x8000u) != 0
                    ? static_cast<std::int32_t>(*raw_node_index) - 0x10000
                    : static_cast<std::int32_t>(*raw_node_index);
                if (signed_node_index < -1 || signed_node_index >= static_cast<std::int32_t>(clip.m_nodes.size()))
                {
                    throw std::runtime_error("RFSM reverse binding references an invalid node");
                }
                reverse.push_back(static_cast<std::int16_t>(signed_node_index));
            }

            for (std::size_t bone = 0; bone < reverse.size(); ++bone)
            {
                const auto node_index = reverse[bone];
                if (node_index >= 0)
                {
                    auto& node = clip.m_nodes[static_cast<std::size_t>(node_index)];
                    if (!node.m_name.empty() && node.m_name != result.m_bone_names[bone])
                    {
                        throw std::runtime_error("RFSM bone bindings assign conflicting names to one MOT2 node");
                    }
                    node.m_name = result.m_bone_names[bone];
                }
            }

            std::size_t channel_base = 0;
            for (std::size_t type = 0; type < 3; ++type)
            {
                for (std::size_t node_index = 0; node_index < clip.m_nodes.size(); ++node_index)
                {
                    const auto& node = clip.m_nodes[node_index];
                    const auto& track = type == 0 ? node.m_translation :
                        type == 1 ? node.m_rotation : node.m_scale;
                    if (track.m_is_animated)
                    {
                        const std::size_t channel = channel_base + track.m_channel_index;
                        const auto bone = forward[channel];
                        if (reverse[bone] != static_cast<std::int16_t>(node_index))
                        {
                            throw std::runtime_error("RFSM forward/reverse bone bindings disagree");
                        }
                    }
                }
                channel_base += clip.m_channel_counts[type];
            }

            result.m_motions.push_back(std::move(clip));
        }

        return result;
    }

    std::array<float, 4> sample_animation_track(
        const s_animation_track& track,
        const float time_seconds,
        const bool quaternion)
    {
        if (track.m_values.empty() || track.m_values.size() != track.m_times.size() || !std::isfinite(time_seconds))
        {
            throw std::runtime_error("cannot sample an empty or invalid animation track");
        }
        if (track.m_values.size() == 1 || time_seconds <= track.m_times.front())
        {
            return quaternion ? normalized_quaternion(track.m_values.front()) : track.m_values.front();
        }
        if (time_seconds >= track.m_times.back())
        {
            return quaternion ? normalized_quaternion(track.m_values.back()) : track.m_values.back();
        }

        const auto upper = std::upper_bound(track.m_times.begin(), track.m_times.end(), time_seconds);
        const std::size_t right_index = static_cast<std::size_t>(std::distance(track.m_times.begin(), upper));
        const std::size_t left_index = right_index - 1;
        const float duration = track.m_times[right_index] - track.m_times[left_index];
        const float amount = (time_seconds - track.m_times[left_index]) / duration;
        const auto& left = track.m_values[left_index];
        const auto& right = track.m_values[right_index];

        if (!quaternion)
        {
            std::array<float, 4> result{};
            for (std::size_t component = 0; component < result.size(); ++component)
            {
                result[component] = left[component] + (right[component] - left[component]) * amount;
            }
            return result;
        }

        auto a = normalized_quaternion(left);
        auto b = normalized_quaternion(right);
        float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
        if (dot < 0.0f)
        {
            dot = -dot;
            for (float& component : b)
            {
                component = -component;
            }
        }
        dot = std::clamp(dot, -1.0f, 1.0f);
        if (dot > 0.9995f)
        {
            std::array<float, 4> result{};
            for (std::size_t component = 0; component < result.size(); ++component)
            {
                result[component] = a[component] + (b[component] - a[component]) * amount;
            }
            return normalized_quaternion(result);
        }

        const float angle = std::acos(dot);
        const float sine = std::sin(angle);
        const float left_weight = std::sin((1.0f - amount) * angle) / sine;
        const float right_weight = std::sin(amount * angle) / sine;
        std::array<float, 4> result{};
        for (std::size_t component = 0; component < result.size(); ++component)
        {
            result[component] = a[component] * left_weight + b[component] * right_weight;
        }
        return normalized_quaternion(result);
    }
}
