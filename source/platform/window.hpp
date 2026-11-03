#pragma once

#include <cstdint>

namespace sdbz
{
}

class c_window
{
public:
    c_window();
    ~c_window();

    c_window(const c_window&) = delete;
    c_window& operator=(const c_window&) = delete;

    bool create(const char* title, int width, int height);
    void destroy();

    void get_size_pixels(int& width, int& height) const;
    void swap_buffers();
    void make_gl_current();
    void set_title(const char* title);

    bool valid() const;

private:
    struct impl;
    impl* m_impl;
};