#pragma once

#include "core/animation.hpp"
#include "gameplay/player_controller.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace sdbz
{
    enum class e_player_animation_state
    {
        idle,
        walk,
        run,
        jump,
        fall,
        land
    };

    [[nodiscard]] e_player_animation_state classify_player_animation_state(
        const s_player_controller& controller, bool landing_override = false);

    [[nodiscard]] std::optional<std::size_t> find_player_animation_clip(
        std::span<const s_animation_clip> clips, e_player_animation_state state);
}
