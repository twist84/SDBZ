#pragma once

#include <cstdint>
#include <string_view>

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

    bool create(std::string_view title, int width, int height);
    void destroy();

    void get_size_pixels(int& width, int& height) const;
    void swap_buffers();
    void make_gl_current();
    void set_title(std::string_view title);

    bool valid() const;

private:
    struct impl;
    impl* m_impl;
};