#include "platform/window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <string>
#include <string_view>

struct c_window::impl
{
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
};

c_window::c_window()
    : m_impl(new impl())
{
}

c_window::~c_window()
{
    destroy();
    delete m_impl;
    m_impl = nullptr;
}

bool c_window::create(std::string_view title, int width, int height)
{
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_impl->window = SDL_CreateWindow(title.data(),
        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_impl->window)
    {
        return false;
    }
    m_impl->gl_context = SDL_GL_CreateContext(m_impl->window);
    if (!m_impl->gl_context)
    {
        SDL_DestroyWindow(m_impl->window);
        m_impl->window = nullptr;
        return false;
    }
    SDL_GL_MakeCurrent(m_impl->window, m_impl->gl_context);
    return true;
}

void c_window::destroy()
{
    if (m_impl->gl_context)
    {
        SDL_GL_DestroyContext(m_impl->gl_context);
        m_impl->gl_context = nullptr;
    }
    if (m_impl->window)
    {
        SDL_DestroyWindow(m_impl->window);
        m_impl->window = nullptr;
    }
}

void c_window::get_size_pixels(int& width, int& height) const
{
    if (m_impl->window)
    {
        SDL_GetWindowSizeInPixels(m_impl->window, &width, &height);
    }
    else
    {
        width = 0;
        height = 0;
    }
}

void c_window::swap_buffers()
{
    if (m_impl->window)
    {
        SDL_GL_SwapWindow(m_impl->window);
    }
}

void c_window::make_gl_current()
{
    if (m_impl->window && m_impl->gl_context)
    {
        SDL_GL_MakeCurrent(m_impl->window, m_impl->gl_context);
    }
}

void c_window::set_title(std::string_view title)
{
    if (m_impl->window)
    {
        SDL_SetWindowTitle(m_impl->window, std::string(title).c_str());
    }
}

bool c_window::valid() const
{
    return m_impl->window != nullptr;
}