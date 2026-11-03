#include "platform/event.hpp"

#include <SDL3/SDL.h>

#include <cstdint>

e_key sdl_key_to_e_key(int key)
{
    if (key >= SDLK_A && key <= SDLK_Z)
    {
        const int idx = key - SDLK_A;
        switch (idx)
        {
            case 0: return e_key::a;
            case 1: return e_key::b;
            case 2: return e_key::c;
            case 3: return e_key::d;
            case 4: return e_key::e;
            case 5: return e_key::f;
            case 6: return e_key::g;
            case 7: return e_key::h;
            case 8: return e_key::i;
            case 9: return e_key::j;
            case 10: return e_key::k;
            case 11: return e_key::l;
            case 12: return e_key::m;
            case 13: return e_key::n;
            case 14: return e_key::o;
            case 15: return e_key::p;
            case 16: return e_key::q;
            case 17: return e_key::r;
            case 18: return e_key::s;
            case 19: return e_key::t;
            case 20: return e_key::u;
            case 21: return e_key::v;
            case 22: return e_key::w;
            case 23: return e_key::x;
            case 24: return e_key::y;
            case 25: return e_key::z;
            default: break;
        }
    }
    if (key >= SDLK_0 && key <= SDLK_9)
    {
        switch (key - SDLK_0)
        {
            case 0: return e_key::digit_0;
            case 1: return e_key::digit_1;
            case 2: return e_key::digit_2;
            case 3: return e_key::digit_3;
            case 4: return e_key::digit_4;
            case 5: return e_key::digit_5;
            case 6: return e_key::digit_6;
            case 7: return e_key::digit_7;
            case 8: return e_key::digit_8;
            case 9: return e_key::digit_9;
            default: break;
        }
    }
    switch (key)
    {
        case SDLK_SPACE: return e_key::space;
        case SDLK_RETURN: return e_key::enter;
        case SDLK_ESCAPE: return e_key::escape;
        case SDLK_HOME: return e_key::home;
        case SDLK_END: return e_key::end;
        case SDLK_UP: return e_key::up;
        case SDLK_DOWN: return e_key::down;
        case SDLK_LEFT: return e_key::left;
        case SDLK_RIGHT: return e_key::right;
        case SDLK_PAGEUP: return e_key::page_up;
        case SDLK_PAGEDOWN: return e_key::page_down;
        case SDLK_F1: return e_key::f1;
        case SDLK_EQUALS: return e_key::equals;
        case SDLK_MINUS: return e_key::minus;
        case SDLK_KP_PLUS: return e_key::kp_plus;
        case SDLK_KP_MINUS: return e_key::kp_minus;
        default: break;
    }
    return e_key::unknown;
}

void poll_events(c_window& window, std::vector<s_event>& events)
{
    events.clear();
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event))
    {
        s_event e;
        switch (sdl_event.type)
        {
            case SDL_EVENT_QUIT:
                e.type = e_event_type::quit;
                break;
            case SDL_EVENT_KEY_DOWN:
                e.type = e_event_type::key_down;
                e.key = sdl_key_to_e_key(sdl_event.key.key);
                e.mod = static_cast<int>(sdl_event.key.mod);
                break;
            case SDL_EVENT_KEY_UP:
                e.type = e_event_type::key_up;
                e.key = sdl_key_to_e_key(sdl_event.key.key);
                e.mod = static_cast<int>(sdl_event.key.mod);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                e.type = e_event_type::mouse_button_down;
                e.mouse_x = static_cast<int>(sdl_event.button.x);
                e.mouse_y = static_cast<int>(sdl_event.button.y);
                if (sdl_event.button.button == SDL_BUTTON_LEFT) e.mouse_button = e_mouse_button::left;
                else if (sdl_event.button.button == SDL_BUTTON_RIGHT) e.mouse_button = e_mouse_button::right;
                else if (sdl_event.button.button == SDL_BUTTON_MIDDLE) e.mouse_button = e_mouse_button::middle;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                e.type = e_event_type::mouse_button_up;
                e.mouse_x = static_cast<int>(sdl_event.button.x);
                e.mouse_y = static_cast<int>(sdl_event.button.y);
                if (sdl_event.button.button == SDL_BUTTON_LEFT) e.mouse_button = e_mouse_button::left;
                else if (sdl_event.button.button == SDL_BUTTON_RIGHT) e.mouse_button = e_mouse_button::right;
                else if (sdl_event.button.button == SDL_BUTTON_MIDDLE) e.mouse_button = e_mouse_button::middle;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                e.type = e_event_type::mouse_motion;
                e.mouse_x = static_cast<int>(sdl_event.motion.x);
                e.mouse_y = static_cast<int>(sdl_event.motion.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                e.type = e_event_type::mouse_wheel;
                e.wheel_y = sdl_event.wheel.y;
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                e.type = e_event_type::gamepad_added;
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                e.type = e_event_type::gamepad_removed;
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                e.type = e_event_type::gamepad_button_down;
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                e.type = e_event_type::gamepad_button_up;
                break;
            default:
                continue;
        }
        events.push_back(e);
    }
    (void)window;
}
