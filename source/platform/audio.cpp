#include "platform/audio.hpp"

#include <SDL3/SDL.h>

struct c_audio_device::impl
{
    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec spec{};
};

c_audio_device::c_audio_device()
    : m_impl(new impl())
{
}

c_audio_device::~c_audio_device()
{
    close();
    delete m_impl;
    m_impl = nullptr;
}

bool c_audio_device::open(int frequency, int channels)
{
    m_impl->spec.freq = frequency;
    m_impl->spec.format = SDL_AUDIO_S16;
    m_impl->spec.channels = static_cast<Uint8>(channels);
    m_impl->device = SDL_OpenAudioDevice(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &m_impl->spec);
    return m_impl->device != 0;
}

void c_audio_device::close()
{
    if (m_impl->device)
    {
        SDL_CloseAudioDevice(m_impl->device);
        m_impl->device = 0;
    }
}

void c_audio_device::play_buffer(const std::int16_t* data, std::size_t count)
{
    (void)data;
    (void)count;
}

bool c_audio_device::valid() const
{
    return m_impl->device != 0;
}