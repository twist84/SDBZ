#include "interface/part_groups.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

std::string part_base_name(const std::string& name)
{
    std::string base = name;
    for (const char* prefix : { "Mesh", "Shape" })
    {
        const std::size_t len = std::strlen(prefix);
        if (base.size() > len && base.compare(0, len, prefix) == 0)
        {
            base.erase(0, len);
            break;
        }
    }
    std::size_t end = base.size();
    while (end > 1 && (std::isupper(static_cast<unsigned char>(base[end - 1])) ||
        std::isdigit(static_cast<unsigned char>(base[end - 1]))))
    {
        --end;
    }
    if (end > 0 && end < base.size() && std::islower(static_cast<unsigned char>(base[end - 1])))
    {
        const std::string suffix = base.substr(end);
        if (suffix != "L" && suffix != "R") base.erase(end);
    }
    return base.empty() ? name : base;
}

void apply_part_visibility(const std::vector<s_part_group>& groups, std::vector<bool>& visible)
{
    std::fill(visible.begin(), visible.end(), false);
    for (const auto& group : groups)
    {
        for (std::size_t i = 0; i < group.m_meshes.size(); ++i)
        {
            visible[group.m_meshes[i]] = (i == group.m_active);
        }
    }
}