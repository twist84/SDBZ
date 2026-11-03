#pragma once

#include <cstddef>
#include <cstdint>

class c_audio_device
{
public:
    c_audio_device();
    ~c_audio_device();

    c_audio_device(const c_audio_device&) = delete;
    c_audio_device& operator=(const c_audio_device&) = delete;

    bool open(int frequency, int channels);
    void close();

    void play_buffer(const std::int16_t* data, std::size_t count);

    bool valid() const;

private:
    struct impl;
    impl* m_impl;
};