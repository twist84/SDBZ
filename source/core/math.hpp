#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace sdbz
{
    using mat4 = std::array<float, 16>;

    [[nodiscard]] inline mat4 mat4_mul(const mat4& a, const mat4& b)
    {
        mat4 out{};
        for (std::size_t row = 0; row < 4; ++row)
        {
            for (std::size_t col = 0; col < 4; ++col)
            {
                float v = 0.0f;
                for (std::size_t k = 0; k < 4; ++k) v += a[row * 4 + k] * b[k * 4 + col];
                out[row * 4 + col] = v;
            }
        }
        return out;
    }

    [[nodiscard]] inline std::array<float, 3> mat4_point(const mat4& m, const std::array<float, 3>& p)
    {
        return {
            m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3],
            m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7],
            m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]
        };
    }

    // Local transform T * Rz * Ry * Rx * S with Euler angles in radians.
    [[nodiscard]] inline mat4 trs_matrix(const std::array<float, 3>& t,
        const std::array<float, 3>& euler, const std::array<float, 3>& s)
    {
        const float cx = std::cos(euler[0]), sx = std::sin(euler[0]);
        const float cy = std::cos(euler[1]), sy = std::sin(euler[1]);
        const float cz = std::cos(euler[2]), sz = std::sin(euler[2]);
        mat4 m{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        m[0] = cz * cy * s[0];
        m[1] = (-sz * cx + cz * sy * sx) * s[1];
        m[2] = (sz * sx + cz * sy * cx) * s[2];
        m[4] = sz * cy * s[0];
        m[5] = (cz * cx + sz * sy * sx) * s[1];
        m[6] = (-cz * sx + sz * sy * cx) * s[2];
        m[8] = -sy * s[0];
        m[9] = cy * sx * s[1];
        m[10] = cy * cx * s[2];
        m[3] = t[0];
        m[7] = t[1];
        m[11] = t[2];
        return m;
    }
}
