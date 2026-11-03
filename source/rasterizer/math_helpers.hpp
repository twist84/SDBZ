#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/model.hpp"

namespace sdbz
{
}

std::array<float, 16> node_local_mat(const sdbz::s_model_node& n);
std::array<float, 16> multiply4(const std::array<float, 16>& a, const std::array<float, 16>& b);
std::array<float, 3> transform_point(const std::array<float, 16>& m, const std::array<float, 3>& p);
std::vector<std::array<float, 16>> build_world(const std::vector<std::array<float, 16>>& local, const std::vector<sdbz::s_model_node>& nodes);
std::array<float, 16> mat4_inverse(const std::array<float, 16>& m);
std::array<float, 3> skin_position(const std::array<float, 3>& pos, const sdbz::s_model_vertex& v, std::int32_t rigid_node, const std::vector<std::array<float, 16>>& inverse_bind, const std::vector<std::array<float, 16>>& world, const std::vector<sdbz::s_model_node>& nodes);