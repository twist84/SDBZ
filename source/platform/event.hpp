#pragma once

#include <cstdint>
#include <vector>

#include "platform/window.hpp"

enum class e_event_type
{
    quit,
    key_down,
    key_up,
    mouse_button_down,
    mouse_button_up,
    mouse_motion,
    mouse_wheel,
    gamepad_added,
    gamepad_removed,
    gamepad_button_down,
    gamepad_button_up
};

enum class e_key
{
    unknown,
    a, b, c, d, e, f, g, h, i, j, k, l, m,
    n, o, p, q, r, s, t, u, v, w, x, y, z,
    digit_0, digit_1, digit_2, digit_3, digit_4,
    digit_5, digit_6, digit_7, digit_8, digit_9,
    space, enter, escape, home, end,
    up, down, left, right,
    page_up, page_down, f1,
    equals, minus,
    kp_plus, kp_minus,
    n_key, b_key, e_key, c_key, t_key, f_key
};

enum class e_mouse_button
{
    left,
    right,
    middle
};

struct s_event
{
    e_event_type type = e_event_type::quit;
    e_key key = e_key::unknown;
    int mod = 0;
    int mouse_x = 0;
    int mouse_y = 0;
    e_mouse_button mouse_button = e_mouse_button::left;
    float wheel_y = 0.0f;
};

e_key sdl_key_to_e_key(int key);

void poll_events(c_window& window, std::vector<s_event>& events);