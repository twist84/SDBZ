#include "rasterizer/math_helpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

std::array<float, 16> node_local_mat(const sdbz::s_model_node& n)
{
    constexpr float k_deg_to_rad = 0.01745329251994329577f;
    const float x = n.m_rotation_degrees[0] * k_deg_to_rad * 0.5f;
    const float y = n.m_rotation_degrees[1] * k_deg_to_rad * 0.5f;
    const float z = n.m_rotation_degrees[2] * k_deg_to_rad * 0.5f;
    const float sx = std::sin(x), sy = std::sin(y), sz = std::sin(z);
    const float cx = std::cos(x), cy = std::cos(y), cz = std::cos(z);
    const float qx = sx * cy * cz - cx * sy * sz;
    const float qy = cx * sy * cz + sx * cy * sz;
    const float qz = cx * cy * sz - sx * sy * cz;
    const float qw = cx * cy * cz + sx * sy * sz;
    const float x2 = qx * qx, y2 = qy * qy, z2 = qz * qz;
    const float xy = qx * qy, xz = qx * qz, xw = qx * qw;
    const float yz = qy * qz, yw = qy * qw;
    const float zw = qz * qw;
    std::array<float, 16> r{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    r[0] = n.m_scale[0] * (1 - 2 * (y2 + z2));
    r[1] = n.m_scale[1] * 2 * (xy - zw);
    r[2] = n.m_scale[2] * 2 * (xz + yw);
    r[4] = n.m_scale[0] * 2 * (xy + zw);
    r[5] = n.m_scale[1] * (1 - 2 * (x2 + z2));
    r[6] = n.m_scale[2] * 2 * (yz - xw);
    r[8] = n.m_scale[0] * 2 * (xz - yw);
    r[9] = n.m_scale[1] * 2 * (yz + xw);
    r[10] = n.m_scale[2] * (1 - 2 * (x2 + y2));
    r[3] = n.m_translation[0];
    r[7] = n.m_translation[1];
    r[11] = n.m_translation[2];
    return r;
}

std::array<float, 16> multiply4(const std::array<float, 16>& a,
    const std::array<float, 16>& b)
{
    std::array<float, 16> out{};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            float v = 0.0f;
            for (int k = 0; k < 4; ++k) v += a[row * 4 + k] * b[k * 4 + col];
            out[row * 4 + col] = v;
        }
    }
    return out;
}

std::array<float, 3> transform_point(const std::array<float, 16>& m,
    const std::array<float, 3>& p)
{
    return std::array<float, 3>{ {
        m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3],
        m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7],
        m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]
    } };
}

std::vector<std::array<float, 16>> build_world(
    const std::vector<std::array<float, 16>>& local,
    const std::vector<sdbz::s_model_node>& nodes)
{
    const std::size_t count = local.size();
    std::vector<std::array<float, 16>> w(count);
    std::vector<std::uint8_t> state(count, 0);
    for (std::size_t start = 0; start < count; ++start)
    {
        if (state[start] == 2) continue;
        std::vector<std::size_t> stack;
        stack.push_back(start);
        while (!stack.empty())
        {
            const std::size_t index = stack.back();
            if (state[index] == 2)
            {
                stack.pop_back();
                continue;
            }
            if (state[index] == 1)
            {
                const auto local_mat = local[index];
                const auto& node = nodes[index];
                if (node.m_parent >= 0)
                {
                    const std::size_t parent = static_cast<std::size_t>(node.m_parent);
                    w[index] = multiply4(w[parent], local_mat);
                }
                else
                {
                    w[index] = local_mat;
                }
                state[index] = 2;
                stack.pop_back();
                continue;
            }
            if (state[index] == 0)
            {
                const auto& node = nodes[index];
                if (node.m_parent >= 0)
                {
                    const std::size_t parent = static_cast<std::size_t>(node.m_parent);
                    if (parent >= count) throw std::runtime_error("invalid parent");
                    if (state[parent] == 1) throw std::runtime_error("hierarchy cycle");
                    if (state[parent] == 0) stack.push_back(parent);
                }
                state[index] = 1;
                continue;
            }
        }
    }
    return w;
}

std::array<float, 16> mat4_inverse(const std::array<float, 16>& m)
{
    std::array<std::array<float, 8>, 4> aug{};
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c) aug[r][c] = m[r * 4 + c];
        aug[r][4 + r] = 1.0f;
    }
    for (int c = 0; c < 4; ++c)
    {
        int piv = c; float maxv = std::fabs(aug[c][c]);
        for (int r = c + 1; r < 4; ++r)
            if (std::fabs(aug[r][c]) > maxv) { maxv = std::fabs(aug[r][c]); piv = r; }
        if (maxv < 1e-6f) return std::array<float, 16>{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        if (piv != c) for (int k = 0; k < 8; ++k) std::swap(aug[c][k], aug[piv][k]);
        float inv = 1.0f / aug[c][c];
        for (int k = 0; k < 8; ++k) aug[c][k] *= inv;
        for (int r = 0; r < 4; ++r)
        {
            if (r == c) continue;
            float f = aug[r][c];
            if (f == 0.0f) continue;
            for (int k = 0; k < 8; ++k) aug[r][k] -= f * aug[c][k];
        }
    }
    std::array<float, 16> inv{};
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) inv[r * 4 + c] = aug[r][c + 4];
    return inv;
}

std::array<float, 3> skin_position(const std::array<float, 3>& pos,
    const sdbz::s_model_vertex& v, std::int32_t rigid_node,
    const std::vector<std::array<float, 16>>& inverse_bind,
    const std::vector<std::array<float, 16>>& world,
    const std::vector<sdbz::s_model_node>& nodes)
{
    std::array<float, 3> out{ 0.0f, 0.0f, 0.0f };
    float total_weight = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float w = v.m_weights[i];
        if (w <= 0.0f) continue;
        const std::uint16_t joint = v.m_joints[i];
        if (joint >= nodes.size()) continue;
        const auto bone_local = transform_point(inverse_bind[joint], pos);
        const auto skinned = transform_point(world[joint], bone_local);
        out[0] += w * skinned[0];
        out[1] += w * skinned[1];
        out[2] += w * skinned[2];
        total_weight += w;
    }
    if (total_weight > 0.0f)
    {
        out[0] /= total_weight;
        out[1] /= total_weight;
        out[2] /= total_weight;
    }
    else
    {
        std::size_t joint = rigid_node >= 0 ? static_cast<std::size_t>(rigid_node) : 0;
        if (joint >= nodes.size()) joint = 0;
        out = transform_point(world[joint], pos);
    }
    return out;
}