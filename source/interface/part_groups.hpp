#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct s_part_group
{
    std::string m_label;
    std::vector<std::size_t> m_meshes;
    std::size_t m_active = 0;
};

std::string part_base_name(const std::string& name);

void apply_part_visibility(const std::vector<s_part_group>& groups, std::vector<bool>& visible);