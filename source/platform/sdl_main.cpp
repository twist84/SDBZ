#include <SDL3/SDL.h>

#include "core/binary_reader.hpp"
#include "core/texture.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    constexpr int k_logical_width = 1120;
    constexpr int k_logical_height = 490;

    struct s_fighter_preview
    {
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_vx = 0.0f;
        bool m_facing_right = true;
    };

    struct s_input_state
    {
        bool m_move_left = false;
        bool m_move_right = false;
        bool m_jump = false;
        bool m_attack = false;
    };

    void set_draw_colour(SDL_Renderer* renderer, std::uint8_t r,
        std::uint8_t g, std::uint8_t b, std::uint8_t a)
    {
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
    }

    void fill_rect(SDL_Renderer* renderer, float x, float y, float w,
        float h)
    {
        SDL_FRect rect{ x, y, w, h };
        SDL_RenderFillRect(renderer, &rect);
    }

    void render_fighter(SDL_Renderer* renderer, const s_fighter_preview& fighter)
    {
        const float size = 40.0f;
        const float x = fighter.m_x - size * 0.5f;
        const float y = fighter.m_y - size * 0.5f;
        fill_rect(renderer, x, y, size, size);
    }

    void update_fighter(s_fighter_preview& fighter, const s_input_state& input,
        float dt)
    {
        const float speed = 240.0f;
        fighter.m_vx = 0.0f;
        if (input.m_move_left)
        {
            fighter.m_vx = -speed;
            fighter.m_facing_right = false;
        }
        if (input.m_move_right)
        {
            fighter.m_vx = speed;
            fighter.m_facing_right = true;
        }
        fighter.m_x += fighter.m_vx * dt;
        if (fighter.m_x < 0.0f)
        {
            fighter.m_x = 0.0f;
        }
        if (fighter.m_x > static_cast<float>(k_logical_width))
        {
            fighter.m_x = static_cast<float>(k_logical_width);
        }
    }

    void update_key(s_input_state& input, const SDL_Keycode key,
        const bool pressed)
    {
        switch (key)
        {
        case SDLK_A:
        case SDLK_LEFT:
            input.m_move_left = pressed;
            break;
        case SDLK_D:
        case SDLK_RIGHT:
            input.m_move_right = pressed;
            break;
        case SDLK_SPACE:
        case SDLK_UP:
            input.m_jump = pressed;
            break;
        case SDLK_J:
            input.m_attack = pressed;
            break;
        default:
            break;
        }
    }

    void update_gamepad_button(s_input_state& input,
        const SDL_GamepadButton button, const bool pressed)
    {
        switch (button)
        {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            input.m_move_left = pressed;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            input.m_move_right = pressed;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
        case SDL_GAMEPAD_BUTTON_SOUTH:
            input.m_jump = pressed;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            input.m_attack = pressed;
            break;
        default:
            break;
        }
    }

    void render_scene(SDL_Renderer* renderer, const s_fighter_preview& fighter,
        bool has_texture, SDL_Texture* texture)
    {
        set_draw_colour(renderer, 12, 16, 24, 255);
        SDL_RenderClear(renderer);

        if (has_texture && texture)
        {
            float texture_width = 0.0f;
            float texture_height = 0.0f;
            SDL_GetTextureSize(texture, &texture_width, &texture_height);
            if (texture_width > 0.0f && texture_height > 0.0f)
            {
                const float scale = std::min(static_cast<float>(k_logical_width) /
                    texture_width,
                    static_cast<float>(k_logical_height) /
                    texture_height);
                const float draw_width = texture_width * scale;
                const float draw_height = texture_height * scale;
                const float draw_x =
                    (static_cast<float>(k_logical_width) - draw_width) * 0.5f;
                const float draw_y =
                    (static_cast<float>(k_logical_height) - draw_height) * 0.5f;
                SDL_FRect dest{ draw_x, draw_y, draw_width, draw_height };
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
                SDL_RenderTexture(renderer, texture, nullptr, &dest);
            }
        }
        else
        {
            set_draw_colour(renderer, 40, 50, 70, 255);
            fill_rect(renderer, 0.0f, k_logical_height * 0.5f - 40.0f,
                static_cast<float>(k_logical_width), 80.0f);

            render_fighter(renderer, fighter);
            set_draw_colour(renderer, 200, 200, 200, 255);
            fill_rect(renderer, k_logical_width * 0.5f - 20.0f,
                k_logical_height * 0.5f + 20.0f, 40.0f, 40.0f);
        }

        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char** argv)
{
    const std::filesystem::path input_path = argc > 1 ? argv[1] : "assets/extracted";
    std::cout << "Input: " << input_path.string() << '\n';

    sdbz::s_texture_image loaded_image;
    bool has_loaded_image = false;
    SDL_Texture* texture = nullptr;
    if (argc > 1 && std::filesystem::is_regular_file(input_path))
    {
        try
        {
            const auto file_data = sdbz::read_file(input_path);
            loaded_image = sdbz::decode_first_fpix_texture(file_data);
            has_loaded_image = true;
            std::cout << "Loaded texture: " << loaded_image.m_width << "x"
                << loaded_image.m_height << '\n';
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Texture preview unavailable: " << exception.what() << '\n';
            return 2;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Super Dragon Ball Z - Native Rebuild",
            k_logical_width, k_logical_height, SDL_WINDOW_RESIZABLE,
            &window, &renderer))
    {
        std::cerr << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError()
            << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, k_logical_width, k_logical_height,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    if (has_loaded_image)
    {
        SDL_Surface* surface = SDL_CreateSurface(
            loaded_image.m_width, loaded_image.m_height, SDL_PIXELFORMAT_RGBA32);
        if (surface)
        {
            std::memcpy(surface->pixels, loaded_image.m_rgba.data(),
                loaded_image.m_rgba.size());
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_DestroySurface(surface);
            if (!texture)
            {
                std::cerr << "SDL_CreateTextureFromSurface failed: "
                    << SDL_GetError() << '\n';
            }
        }
        else
        {
            std::cerr << "SDL_CreateSurface failed: " << SDL_GetError() << '\n';
        }
    }

    SDL_Gamepad* gamepad = SDL_OpenGamepad(0);
    if (!gamepad)
    {
        std::cerr << "No gamepad available (keyboard only).\n";
    }

    s_input_state input;
    s_fighter_preview fighter;
    fighter.m_x = k_logical_width * 0.5f;
    fighter.m_y = k_logical_height * 0.5f;

    constexpr double k_simulation_rate = 60.0;
    constexpr double k_simulation_step = 1.0 / k_simulation_rate;
    double accumulator = 0.0;
    auto previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    bool running = true;
    while (running)
    {
        auto now = SDL_GetPerformanceCounter();
        double frame_time =
            (static_cast<double>(now - previous) / frequency);
        previous = now;
        if (frame_time > 0.25)
        {
            frame_time = 0.25;
        }
        accumulator += frame_time;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                }
                update_key(input, event.key.key, true);
                break;
            case SDL_EVENT_KEY_UP:
                update_key(input, event.key.key, false);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (!gamepad)
                {
                    gamepad = SDL_OpenGamepad(event.gdevice.which);
                    if (!gamepad)
                    {
                        std::cerr << "SDL_OpenGamepad failed: " << SDL_GetError()
                            << '\n';
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (gamepad && SDL_GetGamepadID(gamepad) == event.gdevice.which)
                {
                    SDL_CloseGamepad(gamepad);
                    gamepad = nullptr;
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                update_gamepad_button(input, static_cast<SDL_GamepadButton>(
                    event.gbutton.button), true);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                update_gamepad_button(input, static_cast<SDL_GamepadButton>(
                    event.gbutton.button), false);
                break;
            default:
                break;
            }
        }

        while (accumulator >= k_simulation_step)
        {
            update_fighter(fighter, input, static_cast<float>(k_simulation_step));
            accumulator -= k_simulation_step;
        }

        render_scene(renderer, fighter, has_loaded_image, texture);

        SDL_Delay(1);
    }

    if (gamepad)
    {
        SDL_CloseGamepad(gamepad);
    }
    if (texture)
    {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
