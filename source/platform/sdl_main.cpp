#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "core/animation.hpp"
#include "core/binary_reader.hpp"
#include "core/container.hpp"
#include "core/model.hpp"
#include "core/stage.hpp"
#include "core/texture.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr int k_logical_width = 1120;
    constexpr int k_logical_height = 490;

    struct s_input_state
    {
        bool m_move_left = false;
        bool m_move_right = false;
        bool m_jump = false;
        bool m_attack = false;
    };

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

    // Upload an s_texture_image to OpenGL and return the texture name.
    GLuint upload_texture(const sdbz::s_texture_image& image)
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(image.m_width),
            static_cast<GLsizei>(image.m_height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
            image.m_rgba.data());
        return tex;
    }


// Simple 5x7 bitmap font for text rendering
// Each character is 5 pixels wide, 7 pixels tall, stored as 5 bytes (one per column)
// Bit 0 (LSB) = top row, bit 6 (MSB) = bottom row
constexpr std::array<std::array<uint8_t, 5>, 96> k_font = {{
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space (0x20)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x0C, 0x14, 0x24, 0x7F, 0x04}, // 4
    {0x72, 0x51, 0x51, 0x49, 0x3E}, // 5
    {0x1E, 0x51, 0x51, 0x49, 0x4E}, // 6
    {0x42, 0x41, 0x41, 0x41, 0x40}, // 7
    {0x3E, 0x51, 0x51, 0x51, 0x3E}, // 8
    {0x32, 0x51, 0x51, 0x51, 0x3E}, // 9
    {0x00, 0x7E, 0x11, 0x11, 0x1E}, // :
    {0x00, 0x36, 0x14, 0x00, 0x00}, // ;
    {0x00, 0x08, 0x14, 0x22, 0x41}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x3E, 0x41, 0x5D, 0x55, 0x1E}, // @
    {0x7E, 0x09, 0x09, 0x09, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x41, 0x3E}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x3A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x41, 0x7F, 0x41}, // I
    {0x30, 0x40, 0x40, 0x40, 0x3F}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x10, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // ]
    {0x08, 0x04, 0x02, 0x04, 0x08}, // _
    {0x00, 0x04, 0x06, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x04, 0x04, 0x7E, 0x05, 0x04}, // f
    {0x18, 0x54, 0x54, 0x54, 0x7C}, // g
    {0x7F, 0x04, 0x04, 0x44, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x40, 0x40, 0x44, 0x44, 0x3C}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x82}, // k
    {0x00, 0x41, 0x41, 0x7F, 0x40}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x04, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x54, 0x54, 0x54, 0x28}, // p
    {0x28, 0x54, 0x54, 0x54, 0x7C}, // q
    {0x1C, 0x54, 0x54, 0x54, 0x0C}, // r
    {0x14, 0x54, 0x54, 0x54, 0x4C}, // s
    {0x00, 0x00, 0x7E, 0x04, 0x04}, // t
    {0x3C, 0x40, 0x40, 0x40, 0x7C}, // u
    {0x0C, 0x60, 0x80, 0x60, 0x0C}, // v
    {0x7C, 0x60, 0x10, 0x60, 0x7C}, // w
    {0x66, 0x18, 0x08, 0x18, 0x66}, // x
    {0x0C, 0x40, 0x40, 0x40, 0x7C}, // y
    {0x74, 0x54, 0x54, 0x54, 0x6C}, // z
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // \
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // ]
    {0x08, 0x04, 0x02, 0x04, 0x08}, // _
    {0x20, 0x10, 0x08, 0x10, 0x20}, // `
}};

void draw_text(std::span<const char> text, float x, float y, float scale,
    std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
    glBegin(GL_QUADS);
    float cursor_x = x;
    for (char ch : text) {
        if (ch < 32 || ch > 127) ch = 32; // space fallback
        const auto& glyph_data = k_font[ch - 32];
        for (int col = 0; col < 5; ++col) {
            uint8_t col_bits = glyph_data[col];
            for (int row = 0; row < 7; ++row) {
                if (col_bits & (1 << row)) {
                    glVertex2f(cursor_x + col * scale, y + row * scale);
                    glVertex2f(cursor_x + (col + 1) * scale, y + row * scale);
                    glVertex2f(cursor_x + (col + 1) * scale, y + (row + 1) * scale);
                    glVertex2f(cursor_x + col * scale, y + (row + 1) * scale);
                }
            }
        }
        cursor_x += 6.0f * scale;
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

    // Load every XPIX texture chunk from an FPIX container into OpenGL textures,
    // returning texture names indexed by texture slot.
    std::vector<GLuint> load_fpix_textures(const std::vector<std::uint8_t>& data)
    {
        std::vector<GLuint> textures;
        if (data.size() < 32 || std::string_view(reinterpret_cast<const char*>(data.data()), 4) != "FPIX")
        {
            return textures;
        }
        const auto directory = sdbz::parse_chunked_container(data, "FPIX");
        for (const auto& chunk : directory.m_chunks)
        {
            if (chunk.magic_string() != "XPIX")
            {
                continue;
            }
            const std::span<const std::uint8_t> chunk_data(data.data() + chunk.m_offset, chunk.m_size);
            try
            {
                const auto image = sdbz::decode_xpix(chunk_data);
                textures.push_back(upload_texture(image));
                std::cout << "Loaded texture " << textures.size() << " from XPIX chunk "
                    << chunk.m_offset << " (" << image.m_width << "x" << image.m_height << ")\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "XPIX decode failed: " << exception.what() << '\n';
            }
        }
        return textures;
    }

    // Camera state for orbiting the model.
    struct s_camera
    {
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
        float m_distance = 6.0f;
        std::array<float, 3> m_target{ 0.0f, 0.0f, 0.0f };
    };

    void apply_camera(const s_camera& camera, int drawable_width, int drawable_height)
    {
        const float aspect = static_cast<float>(drawable_width) /
            static_cast<float>(drawable_height > 0 ? drawable_height : 1);
        const float view_size = camera.m_distance;
        const float left = -aspect * view_size;
        const float right = aspect * view_size;
        const float bottom = -view_size;
        const float top = view_size;
        const float z_near = -1000.0f;
        const float z_far = 1000.0f;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(left, right, bottom, top, z_near, z_far);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(-camera.m_target[0], -camera.m_target[1], -camera.m_target[2] - camera.m_distance);
        glRotatef(-camera.m_pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-camera.m_yaw, 0.0f, 1.0f, 0.0f);
    }
}

int main(int argc, char** argv)
{
    const std::filesystem::path input_path = argc > 1 ? argv[1] : "assets/extracted";
    std::cout << "Input: " << input_path.string() << '\n';

    bool is_pix = false;
    bool is_mdl = false;
    bool is_hit = false;
    sdbz::s_texture_image loaded_image;
    bool has_loaded_image = false;
    sdbz::s_model loaded_model;
    bool has_loaded_model = false;
    sdbz::s_motion_package loaded_motion;
    bool has_loaded_motion = false;
    float animation_time = 0.0f;
    std::size_t motion_clip_index = 0;
    sdbz::s_hit loaded_hit;
    bool has_loaded_hit = false;
    std::vector<GLuint> texture_names;
    bool has_textures = false;
    std::vector<std::string> mesh_filters;

    const std::filesystem::path motion_path = argc > 2 ? argv[2] : "";
    const std::filesystem::path texture_path = argc > 3 ? argv[3] : "";

    for (int i = 4; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--mesh" && i + 1 < argc)
        {
            mesh_filters.push_back(argv[++i]);
        }
    }

    if (argc > 1 && std::filesystem::is_regular_file(input_path))
    {
        const std::string ext = input_path.extension().string();
        if (ext == ".pix" || ext == ".PIX")
        {
            is_pix = true;
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
        else if (ext == ".mdl" || ext == ".MDL")
        {
            is_mdl = true;
            try
            {
                const auto file_data = sdbz::read_file(input_path);
                loaded_model = sdbz::parse_fmdl(file_data, input_path.stem().string());
                has_loaded_model = true;
                std::cout << "Loaded model: " << loaded_model.m_name
                    << " (meshes=" << loaded_model.m_meshes.size()
                    << " materials=" << loaded_model.m_materials.size()
                    << " nodes=" << loaded_model.m_nodes.size() << ")\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Model load unavailable: " << exception.what() << '\n';
                return 2;
            }
        }
        else if (ext == ".hit" || ext == ".HIT")
        {
            is_hit = true;
            try
            {
                const auto file_data = sdbz::read_file(input_path);
                loaded_hit = sdbz::parse_hit(file_data);
                has_loaded_hit = true;
                std::cout << "Loaded hit: " << loaded_hit.m_objects.size() << " objects, "
                    << loaded_hit.m_polygons.size() << " polys, "
                    << loaded_hit.m_vertices.size() << " verts\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Hit load unavailable: " << exception.what() << '\n';
                return 2;
            }
        }
    }

    if (argc > 2 && std::filesystem::is_regular_file(motion_path))
    {
        const std::string ext = motion_path.extension().string();
        if (ext == ".pmt" || ext == ".PMT" || ext == ".mot" || ext == ".MOT")
        {
            try
            {
                const auto file_data = sdbz::read_file(motion_path);
                loaded_motion = sdbz::parse_rfsm(file_data);
                has_loaded_motion = true;
                std::cout << "Loaded motion package: " << loaded_motion.m_motions.size()
                    << " motions, " << loaded_motion.m_bone_names.size() << " bones\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Motion load unavailable: " << exception.what() << '\n';
                return 2;
            }
        }
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("Super Dragon Ball Z - Native Rebuild (OpenGL)",
        k_logical_width, k_logical_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
    {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    glEnable(GL_DEPTH_TEST);

    // Load textures AFTER OpenGL context is created
    if (argc > 3 && std::filesystem::is_regular_file(texture_path))
    {
        const std::string ext = texture_path.extension().string();
        if (ext == ".pix" || ext == ".PIX")
        {
            try
            {
                const auto file_data = sdbz::read_file(texture_path);
                texture_names = load_fpix_textures(file_data);
                has_textures = !texture_names.empty();
                std::cout << "Loaded " << texture_names.size() << " textures from "
                    << texture_path.string() << '\n';
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Texture load unavailable: " << exception.what() << '\n';
            }
        }
    }

    SDL_Gamepad* gamepad = SDL_OpenGamepad(0);
    if (!gamepad)
    {
        std::cerr << "No gamepad available (keyboard only).\n";
    }

    s_input_state input;
    s_camera camera;

    constexpr double k_simulation_rate = 60.0;
    constexpr double k_simulation_step = 1.0 / k_simulation_rate;
    double accumulator = 0.0;
    auto previous = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    bool running = true;
    bool left_button_down = false;
    int last_mouse_x = 0;
    int last_mouse_y = 0;
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
                if (has_loaded_motion)
                {
                    if (event.key.key == SDLK_N)
                    {
                        motion_clip_index = (motion_clip_index + 1) % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        std::cout << "Motion clip " << motion_clip_index << ": "
                            << loaded_motion.m_motions[motion_clip_index].m_name << '\n';
                    }
                    else if (event.key.key == SDLK_B)
                    {
                        motion_clip_index = (motion_clip_index + loaded_motion.m_motions.size() - 1)
                            % loaded_motion.m_motions.size();
                        animation_time = 0.0f;
                        std::cout << "Motion clip " << motion_clip_index << ": "
                            << loaded_motion.m_motions[motion_clip_index].m_name << '\n';
                    }
                    else if (event.key.key == SDLK_EQUALS ||
                        event.key.key == SDLK_KP_PLUS)
                    {
                        camera.m_distance = std::max(1.0f, camera.m_distance - 0.5f);
                    }
                    else if (event.key.key == SDLK_MINUS ||
                        event.key.key == SDLK_KP_MINUS)
                    {
                        camera.m_distance += 0.5f;
                    }
                }
                update_key(input, event.key.key, true);
                break;
            case SDL_EVENT_KEY_UP:
                update_key(input, event.key.key, false);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_button_down = true;
                    last_mouse_x = event.button.x;
                    last_mouse_y = event.button.y;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    left_button_down = false;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (left_button_down)
                {
                    const int dx = event.motion.x - last_mouse_x;
                    const int dy = event.motion.y - last_mouse_y;
                    camera.m_yaw += dx * 0.5f;
                    camera.m_pitch = std::clamp(camera.m_pitch + dy * 0.5f, -85.0f, 85.0f);
                    last_mouse_x = event.motion.x;
                    last_mouse_y = event.motion.y;
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                camera.m_distance = std::clamp(camera.m_distance - event.wheel.y * 0.5f, 1.0f, 50.0f);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (!gamepad)
                {
                    gamepad = SDL_OpenGamepad(event.gdevice.which);
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
            accumulator -= k_simulation_step;
        }

        int drawable_width = 0;
        int drawable_height = 0;
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        glViewport(0, 0, drawable_width, drawable_height);

        glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        apply_camera(camera, drawable_width, drawable_height);

        if (has_loaded_model && !loaded_model.m_meshes.empty())
        {
            // compute bind-pose world transforms (row-major-ish column array)
            const auto node_local = [](const sdbz::s_model_node& n) -> std::array<float, 16>
            {
                constexpr float k_deg_to_rad = 0.01745329251994329577f;
                const float x = n.m_rotation_degrees[0] * k_deg_to_rad * 0.5f;
                const float y = n.m_rotation_degrees[1] * k_deg_to_rad * 0.5f;
                const float z = n.m_rotation_degrees[2] * k_deg_to_rad * 0.5f;
                const float sx = std::sin(x), sy = std::sin(y), sz = std::sin(z);
                const float cx = std::cos(x), cy = std::cos(y), cz = std::cos(z);
                // Quaternion from Euler (matches arika-py quaternion function)
                const float qx = sx * cy * cz - cx * sy * sz;
                const float qy = cx * sy * cz + sx * cy * sz;
                const float qz = cx * cy * sz - sx * sy * cz;
                const float qw = cx * cy * cz + sx * sy * sz;
                // Quaternion to matrix (matches arika-py local_matrix)
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
            };
            const auto multiply4 = [](const std::array<float,16>& a,
                const std::array<float,16>& b) -> std::array<float,16>
            {
                std::array<float,16> out{};
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
            };
            const auto transform_point = [](const std::array<float,16>& m,
                const std::array<float,3>& p) -> std::array<float,3>
            {
                return std::array<float,3>{{
                    m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3],
                    m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7],
                    m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]
                }};
            };
            const auto build_world = [&](const std::vector<std::array<float,16>>& local)
                -> std::vector<std::array<float,16>>
            {
                std::vector<std::array<float,16>> w(local.size());
                std::vector<char> done(local.size(), 0);
                std::function<std::array<float,16>(std::size_t)> build =
                    [&](std::size_t index) -> std::array<float,16>
                {
                    if (done[index] == 2) return w[index];
                    if (done[index] == 1) throw std::runtime_error("hierarchy cycle");
                    done[index] = 1;
                    const auto local_mat = local[index];
                    const auto& node = loaded_model.m_nodes[index];
                    std::array<float,16> result;
                    if (node.m_parent >= 0)
                    {
                        const auto& parent_world = build(static_cast<std::size_t>(node.m_parent));
                        result = multiply4(parent_world, local_mat);
                    }
                    else
                    {
                        result = local_mat;
                    }
                    w[index] = result;
                    done[index] = 2;
                    return result;
                };
                for (std::size_t index = 0; index < local.size(); ++index) build(index);
                return w;
            };
            // advance animation time
            if (has_loaded_motion && motion_clip_index < loaded_motion.m_motions.size())
            {
                animation_time += static_cast<float>(frame_time);
                const auto& clip = loaded_motion.m_motions[motion_clip_index];
                if (animation_time > clip.m_duration)
                {
                    animation_time = 0.0f;
                }
            }

            // local transform per node: animation sample overrides bind pose
            std::vector<std::array<float,16>> local_xform(loaded_model.m_nodes.size());
            for (std::size_t ni = 0; ni < loaded_model.m_nodes.size(); ++ni)
            {
                local_xform[ni] = node_local(loaded_model.m_nodes[ni]);
            }

            // bind pose world transforms (for drawing bones that match mesh vertices)
            std::vector<std::array<float,16>> bind_local(loaded_model.m_nodes.size());
            for (std::size_t ni = 0; ni < loaded_model.m_nodes.size(); ++ni)
            {
                bind_local[ni] = node_local(loaded_model.m_nodes[ni]);
            }
            std::vector<std::array<float,16>> bind_world = build_world(bind_local);
            const auto& bind_world_refs = bind_world;

            const auto mat4_inverse = [](const std::array<float,16>& m) -> std::array<float,16>
            {
                std::array<std::array<float,8>,4> aug{};
                for (int r = 0; r < 4; ++r)
                {
                    for (int c = 0; c < 4; ++c) aug[r][c] = m[r * 4 + c];
                    aug[r][4 + r] = 1.0f;
                }
                for (int c = 0; c < 4; ++c)
                {
                    int piv = c; float maxv = std::fabsf(aug[c][c]);
                    for (int r = c + 1; r < 4; ++r)
                        if (std::fabsf(aug[r][c]) > maxv) { maxv = std::fabsf(aug[r][c]); piv = r; }
                    if (maxv < 1e-6f) return std::array<float,16>{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
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
                std::array<float,16> inv{};
                for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) inv[r * 4 + c] = aug[r][c + 4];
                return inv;
            };
            // inverse bind matrices for skinning: world[joint] * inverse_bind[joint] * pos
            std::vector<std::array<float,16>> inverse_bind(bind_world.size());
            for (std::size_t i = 0; i < bind_world.size(); ++i)
            {
                inverse_bind[i] = mat4_inverse(bind_world[i]);
            }

            if (has_loaded_motion && motion_clip_index < loaded_motion.m_motions.size())
            {
                const auto& clip = loaded_motion.m_motions[motion_clip_index];
                std::unordered_map<std::string, std::size_t> name_to_node;
                for (std::size_t ni = 0; ni < loaded_model.m_nodes.size(); ++ni)
                {
                    name_to_node[loaded_model.m_nodes[ni].m_name] = ni;
                }
                for (const auto& an : clip.m_nodes)
                {
                    auto it = name_to_node.find(an.m_name);
                    if (it == name_to_node.end())
                    {
                        continue;
                    }
                    const std::size_t mi = it->second;
                    std::array<float, 3> trans{ 0.0f, 0.0f, 0.0f };
                    std::array<float, 4> rot{ 0.0f, 0.0f, 0.0f, 1.0f };
                    std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
                    // Animated tracks are sampled; constant tracks hold one float4 in m_values.
                    if (!an.m_translation.m_values.empty())
                    {
                        const auto t = sdbz::sample_animation_track(an.m_translation, animation_time, false);
                        trans = { t[0], t[1], t[2] };
                    }
                    if (!an.m_rotation.m_values.empty())
                    {
                        const auto r = sdbz::sample_animation_track(an.m_rotation, animation_time, true);
                        rot = { r[0], r[1], r[2], r[3] };
                    }
                    if (!an.m_scale.m_values.empty())
                    {
                        const auto s = sdbz::sample_animation_track(an.m_scale, animation_time, false);
                        scale = { s[0], s[1], s[2] };
                    }
                    const float x = rot[0], y = rot[1], z = rot[2], w = rot[3];
                    std::array<float,16> rot_mat{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                    const float xx = x * x, yy = y * y, zz = z * z;
                    const float xy = x * y, xz = x * z, xw = x * w;
                    const float yz = y * z, yw = y * w;
                    const float zw = z * w;
                    rot_mat[0] = 1.0f - 2.0f * (yy + zz);
                    rot_mat[1] = 2.0f * (xy - zw);
                    rot_mat[2] = 2.0f * (xz + yw);
                    rot_mat[4] = 2.0f * (xy + zw);
                    rot_mat[5] = 1.0f - 2.0f * (xx + zz);
                    rot_mat[6] = 2.0f * (yz - xw);
                    rot_mat[8] = 2.0f * (xz - yw);
                    rot_mat[9] = 2.0f * (yz + xw);
                    rot_mat[10] = 1.0f - 2.0f * (xx + yy);
                    std::array<float,16> trans_mat{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
                    trans_mat[3] = trans[0]; trans_mat[7] = trans[1]; trans_mat[11] = trans[2];
                    std::array<float,16> scale_mat{ scale[0],0,0,0, 0,scale[1],0,0, 0,0,scale[2],0, 0,0,0,1 };
                    local_xform[mi] = multiply4(multiply4(trans_mat, rot_mat), scale_mat);
                }
            }

            std::vector<std::array<float,16>> world(loaded_model.m_nodes.size());
            std::vector<char> done(loaded_model.m_nodes.size(), 0);
            std::function<std::array<float,16>(std::size_t)> build =
                [&](std::size_t index) -> std::array<float,16>
            {
                if (done[index] == 2) return world[index];
                if (done[index] == 1) throw std::runtime_error("hierarchy cycle");
                done[index] = 1;
                const auto local = local_xform[index];
                const auto& node = loaded_model.m_nodes[index];
                std::array<float,16> result;
                if (node.m_parent >= 0)
                {
                    const auto& parent_world = build(static_cast<std::size_t>(node.m_parent));
                    result = multiply4(parent_world, local);
                }
                else
                {
                    result = local;
                }
                world[index] = result;
                done[index] = 2;
                return result;
            };
            for (std::size_t index = 0; index < loaded_model.m_nodes.size(); ++index)
            {
                build(index);
            }
            const auto& world_refs = world;

            auto skin_position = [&](const std::array<float, 3>& pos,
                const sdbz::s_model_vertex& v, std::int32_t rigid_node) -> std::array<float, 3>
            {
                // Per arika-py: skinned mesh vertices are in joint-local space at bind pose.
                // Animated: world[joint] * inverse_bind[joint] * pos, weighted by bone weights.
                std::array<float, 3> out{ 0.0f, 0.0f, 0.0f };
                float total_weight = 0.0f;
                for (int i = 0; i < 4; ++i)
                {
                    const float w = v.m_weights[i];
                    if (w <= 0.0f) continue;
                    const std::uint16_t joint = v.m_joints[i];
                    if (joint >= loaded_model.m_nodes.size()) continue;
                    // bind pose -> joint-local (inverse_bind) -> animated world (world[joint])
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
                    // Non-skinned mesh: apply node world transform
                    // Rigid meshes (head, hair, hands...) are attached to the node stored on the
                    // mesh, not in the per-vertex joint array (which is always 0 for them).
                    std::size_t joint = rigid_node >= 0 ? static_cast<std::size_t>(rigid_node) : 0;
                    if (joint >= loaded_model.m_nodes.size()) joint = 0;
                    out = transform_point(world[joint], pos);
                }
                return out;
            };

            // Compute skinned bounding box for camera framing
            std::array<float, 3> skinned_min{ 1e9f, 1e9f, 1e9f };
            std::array<float, 3> skinned_max{ -1e9f, -1e9f, -1e9f };
            bool skinned_has_data = false;
            for (const auto& m : loaded_model.m_meshes)
            {
                if (!mesh_filters.empty())
                {
                    bool matches = false;
                    for (const auto& f : mesh_filters)
                    {
                        if (m.m_name.find(f) != std::string::npos)
                        {
                            matches = true;
                            break;
                        }
                    }
                    if (!matches) continue;
                }
                for (const auto& p : m.m_primitives)
                {
                    if (p.m_indices.size() < 3) continue;
                    for (std::size_t i = 0; i + 2 < p.m_indices.size(); i += 3)
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            const std::uint32_t idx = p.m_indices[i + k];
                            if (idx >= m.m_vertices.size()) continue;
                            const auto& v = m.m_vertices[idx];
                            const auto sp = skin_position(v.m_position, v, m.m_rigid_node_index);
                            skinned_min[0] = std::min(skinned_min[0], sp[0]);
                            skinned_min[1] = std::min(skinned_min[1], sp[1]);
                            skinned_min[2] = std::min(skinned_min[2], sp[2]);
                            skinned_max[0] = std::max(skinned_max[0], sp[0]);
                            skinned_max[1] = std::max(skinned_max[1], sp[1]);
                            skinned_max[2] = std::max(skinned_max[2], sp[2]);
                            skinned_has_data = true;
                        }
                    }
                }
            }
            if (has_loaded_model && skinned_has_data)
            {
                camera.m_target = {
                    (skinned_min[0] + skinned_max[0]) * 0.5f,
                    (skinned_min[1] + skinned_max[1]) * 0.5f,
                    (skinned_min[2] + skinned_max[2]) * 0.5f,
                };
                const float extent = std::max({ skinned_max[0] - skinned_min[0],
                    skinned_max[1] - skinned_min[1],
                    skinned_max[2] - skinned_min[2] });
                camera.m_distance = std::max(extent * 3.0f, 6.0f);
                std::cout << "Skinned bbox: (" << skinned_min[0] << "," << skinned_min[1] << ","
                    << skinned_min[2] << ") to (" << skinned_max[0] << "," << skinned_max[1] << ","
                    << skinned_max[2] << ") extent=" << extent << " distance=" << camera.m_distance << '\n';
            }

            // draw skinned mesh with per-material textures and UVs
            glEnable(GL_LIGHTING);
            glEnable(GL_NORMALIZE);
            glEnable(GL_LIGHT0);
            GLfloat ambient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
            glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
            GLfloat light_pos[] = { 5.0f, 10.0f, 5.0f, 0.0f };
            glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
            GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
            glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
            glColor3f(1.0f, 1.0f, 1.0f);
            for (const auto& mesh : loaded_model.m_meshes)
            {
                if (!mesh_filters.empty())
                {
                    bool matches = false;
                    for (const auto& filter : mesh_filters)
                    {
                        if (mesh.m_name.find(filter) != std::string::npos)
                        {
                            matches = true;
                            break;
                        }
                    }
                    if (!matches) continue;
                }
                for (const auto& primitive : mesh.m_primitives)
                {
                    if (primitive.m_indices.size() < 3) continue;
                    const std::int32_t material_index = primitive.m_material_index;
                    bool                 has_texture = false;
                    GLuint tex = 0;
                    if (has_textures && material_index >= 0 &&
                        static_cast<std::size_t>(material_index) < loaded_model.m_materials.size())
                    {
                        const std::int32_t texture_index =
                            loaded_model.m_materials[material_index].m_texture_index;
                        std::cout << "Material " << material_index << " texture_index="
                            << texture_index << " textures=" << texture_names.size() << '\n';
                        if (texture_index >= 0 &&
                            static_cast<std::size_t>(texture_index) < texture_names.size())
                        {
                            tex = texture_names[texture_index];
                            has_texture = true;
                        }
                    }
                    if (has_texture)
                    {
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, tex);
                    }
                    else
                    {
                        glDisable(GL_TEXTURE_2D);
                    }
                    glBegin(GL_TRIANGLES);
                    for (std::size_t i = 0; i + 2 < primitive.m_indices.size(); i += 3)
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            const std::uint32_t idx = primitive.m_indices[i + k];
                            if (idx >= mesh.m_vertices.size()) continue;
                            const auto& v = mesh.m_vertices[idx];
                            const auto sp = skin_position(v.m_position, v, mesh.m_rigid_node_index);
                            glNormal3f(v.m_normal[0], v.m_normal[1], v.m_normal[2]);
                            if (has_texture)
                            {
                                glTexCoord2f(v.m_uv[0], v.m_uv[1]);
                            }
                            glVertex3f(sp[0], sp[1], sp[2]);
                        }
                    }
                    glEnd();
                    if (has_texture)
                    {
                        glDisable(GL_TEXTURE_2D);
                    }
                }
            }
            glDisable(GL_COLOR_MATERIAL);
        }
        else if (has_loaded_hit)

struct s_glyph {
    float x;
    float y;
    float scale;
    std::uint8_t r, g, b;
};

void draw_text(std::span<const char> text, float x, float y, float scale, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
    glBegin(GL_QUADS);
    float cursor_x = x;
    for (char ch : text) {
        if (ch < 32 || ch > 127) ch = 32; // space fallback
        const auto& glyph_data = k_font[ch - 32];
        for (int col = 0; col < 5; ++col) {
            uint8_t col_bits = glyph_data[col];
            for (int row = 0; row < 7; ++row) {
                if (col_bits & (1 << row)) {
                    glVertex2f(cursor_x + col * scale, y + row * scale);
                    glVertex2f(cursor_x + (col + 1) * scale, y + row * scale);
                    glVertex2f(cursor_x + (col + 1) * scale, y + (row + 1) * scale);
                    glVertex2f(cursor_x + col * scale, y + (row + 1) * scale);
                }
            }
        }
        cursor_x += 6.0f * scale;
    }
    glEnd();
    glEnable(GL_LIGHTING);
}
                            glVertex3f(sp[0], sp[1], sp[2]);
                        }
                    }
                    glEnd();
                    if (has_texture)
                    {
                        glDisable(GL_TEXTURE_2D);
                    }
                }
            }
            glDisable(GL_COLOR_MATERIAL);
        }
        else if (has_loaded_hit)
        {
            glDisable(GL_LIGHTING);
            glColor3f(0.0f, 1.0f, 0.0f);
            glBegin(GL_LINES);
            for (const auto& poly : loaded_hit.m_polygons)
            {
                if (poly.m_vertex_count < 2) continue;
                for (std::uint32_t i = 0; i < poly.m_vertex_count; ++i)
                {
                    const std::uint32_t a = poly.m_first_vertex + i;
                    const std::uint32_t b = poly.m_first_vertex + ((i + 1) % poly.m_vertex_count);
                    if (a >= loaded_hit.m_vertices.size() || b >= loaded_hit.m_vertices.size()) continue;
                    const auto& va = loaded_hit.m_vertices[a].m_position;
                    const auto& vb = loaded_hit.m_vertices[b].m_position;
                    glVertex3fv(&va[0]);
                    glVertex3fv(&vb[0]);
                }
            }
            glEnd();
            glEnable(GL_LIGHTING);
        }
        else if (has_loaded_image && !texture_names.empty())
        {
            // Not used for single-image mode; textures are bound per-material above.
        }
        else if (has_loaded_image && !has_textures)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, upload_texture(loaded_image));

            const float scale = std::min(
                static_cast<float>(drawable_width) / static_cast<float>(loaded_image.m_width),
                static_cast<float>(drawable_height) / static_cast<float>(loaded_image.m_height));
            const float draw_width = static_cast<float>(loaded_image.m_width) * scale;
            const float draw_height = static_cast<float>(loaded_image.m_height) * scale;
            const float draw_x = (static_cast<float>(drawable_width) - draw_width) * 0.5f;
            const float draw_y = (static_cast<float>(drawable_height) - draw_height) * 0.5f;

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, drawable_width, drawable_height, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            glColor3f(1.0f, 1.0f, 1.0f);
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(draw_x, draw_y);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(draw_x + draw_width, draw_y);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(draw_x + draw_width, draw_y + draw_height);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(draw_x, draw_y + draw_height);
            glEnd();
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_LIGHTING);
        }

        glDisable(GL_LIGHTING);
        }

        // On-screen HUD: motion name and mesh filters
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 1.0f, 1.0f);
        std::string hud = "INFO.DAT/GAME.DAT viewer";
        if (has_loaded_model) hud += " | model: " + loaded_model.m_name;
        if (has_loaded_motion) hud += " | motion: " + loaded_motion.m_motions[motion_clip_index].m_name;
        else if (has_loaded_motion == false && argc > 2) hud += " | motion: none";
        if (!mesh_filters.empty())
        {
            hud += " | meshes:";
            for (const auto& f : mesh_filters) hud += " " + f;
        }
        draw_text(hud, 10.0f, 10.0f, 1.5f, 255, 255, 0);

        SDL_GL_SwapWindow(window);
        SDL_Delay(1);
    }

    for (const auto tex : texture_names)
    {
        if (tex) glDeleteTextures(1, &tex);
    }
    if (gamepad)
    {
        SDL_CloseGamepad(gamepad);
    }
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}