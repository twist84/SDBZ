#include "gameplay/player_animation.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace sdbz
{
    namespace
    {
        [[nodiscard]] std::string normalise_name(const std::string_view name)
        {
            std::string result;
            result.reserve(name.size());
            for (const char raw_character : name)
            {
                const auto character = static_cast<unsigned char>(raw_character);
                if (std::isalnum(character) != 0)
                {
                    result.push_back(static_cast<char>(std::tolower(character)));
                }
                else
                {
                    result.push_back('_');
                }
            }
            return result;
        }

        [[nodiscard]] bool contains_any(
            const std::string& name, const std::span<const std::string_view> tokens)
        {
            return std::any_of(tokens.begin(), tokens.end(),
                [&](const std::string_view token)
                {
                    return name.find(token) != std::string::npos;
                });
        }

        [[nodiscard]] bool matches_state(
            const std::string& name, const e_player_animation_state state)
        {
            static constexpr std::array idle_tokens{
                std::string_view("idle"), std::string_view("wait"),
                std::string_view("stand"), std::string_view("stnd")
            };
            static constexpr std::array walk_tokens{
                std::string_view("walk"), std::string_view("move"),
                std::string_view("step")
            };
            static constexpr std::array run_tokens{
                std::string_view("run"), std::string_view("dash"),
                std::string_view("sprint")
            };
            static constexpr std::array jump_tokens{
                std::string_view("jump"), std::string_view("jamp"),
                std::string_view("takeoff")
            };
            static constexpr std::array fall_tokens{
                std::string_view("fall"), std::string_view("airborne"),
                std::string_view("air")
            };
            static constexpr std::array land_tokens{
                std::string_view("land"), std::string_view("touchdown"),
                std::string_view("touch")
            };
            switch (state)
            {
            case e_player_animation_state::idle: return contains_any(name, idle_tokens);
            case e_player_animation_state::walk: return contains_any(name, walk_tokens);
            case e_player_animation_state::run: return contains_any(name, run_tokens);
            case e_player_animation_state::jump: return contains_any(name, jump_tokens);
            case e_player_animation_state::fall: return contains_any(name, fall_tokens);
            case e_player_animation_state::land: return contains_any(name, land_tokens);
            }
            return false;
        }

        [[nodiscard]] std::span<const e_player_animation_state> fallback_states(
            const e_player_animation_state state)
        {
            static constexpr std::array run_fallback{
                e_player_animation_state::walk, e_player_animation_state::idle
            };
            static constexpr std::array walk_fallback{
                e_player_animation_state::idle
            };
            static constexpr std::array fall_fallback{
                e_player_animation_state::jump
            };
            static constexpr std::array land_fallback{
                e_player_animation_state::idle
            };
            switch (state)
            {
            case e_player_animation_state::run: return run_fallback;
            case e_player_animation_state::walk: return walk_fallback;
            case e_player_animation_state::fall: return fall_fallback;
            case e_player_animation_state::land: return land_fallback;
            default: return {};
            }
        }

        [[nodiscard]] std::optional<std::size_t> find_exact_state(
            const std::span<const s_animation_clip> clips,
            const e_player_animation_state state)
        {
            for (std::size_t index = 0; index < clips.size(); ++index)
            {
                const std::string name = normalise_name(clips[index].m_name);
                if (matches_state(name, state))
                {
                    return index;
                }
            }
            return std::nullopt;
        }
    }

    e_player_animation_state classify_player_animation_state(
        const s_player_controller& controller, const bool landing_override)
    {
        if (landing_override)
        {
            return e_player_animation_state::land;
        }
        if (!controller.m_grounded)
        {
            return controller.m_velocity[1] > 0.15f
                ? e_player_animation_state::jump : e_player_animation_state::fall;
        }

        const float horizontal_speed_sq = controller.m_velocity[0] * controller.m_velocity[0] +
            controller.m_velocity[2] * controller.m_velocity[2];
        if (horizontal_speed_sq <= 0.04f)
        {
            return e_player_animation_state::idle;
        }
        return controller.m_running
            ? e_player_animation_state::run : e_player_animation_state::walk;
    }

    std::optional<std::size_t> find_player_animation_clip(
        const std::span<const s_animation_clip> clips,
        const e_player_animation_state state)
    {
        if (const auto exact = find_exact_state(clips, state))
        {
            return exact;
        }
        for (const e_player_animation_state fallback : fallback_states(state))
        {
            if (const auto match = find_exact_state(clips, fallback))
            {
                return match;
            }
        }
        return std::nullopt;
    }
}
