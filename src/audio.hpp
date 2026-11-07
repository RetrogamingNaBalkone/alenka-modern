#pragma once
#include <SDL.h>
#include "sound.hpp"

namespace alenka {
class Audio {
    SDL_AudioDeviceID device_=0;
    const std::vector<std::int16_t> error_=sound_samples(true), beat_=sound_samples(false);
public:
    Audio() {
        // Audio is optional: machines without a device still run the trainer.
        if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,"Sound unavailable: %s",SDL_GetError());
            return;
        }
        SDL_AudioSpec wanted{};
        wanted.freq=48000; wanted.format=AUDIO_S16SYS; wanted.channels=1; wanted.samples=512;
        device_=SDL_OpenAudioDevice(nullptr,0,&wanted,nullptr,0);
        if(device_) SDL_PauseAudioDevice(device_,0);
        else SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,"Sound unavailable: %s",SDL_GetError());
    }
    ~Audio() { if(device_) SDL_CloseAudioDevice(device_); }
    Audio(const Audio&)=delete;
    Audio& operator=(const Audio&)=delete;
    bool available() const { return device_!=0; }
    Uint32 queued_bytes() const { return device_?SDL_GetQueuedAudioSize(device_):0; }
    void stop() { if(device_) SDL_ClearQueuedAudio(device_); }
    void update(bool enabled,bool error,bool beat) {
        if(!device_) return;
        if(!enabled) { stop(); return; }
        // Errors take priority. Never queue a backlog after stalls or rapid input.
        if(error) stop();
        else if(!beat || SDL_GetQueuedAudioSize(device_)) return;
        const auto& samples=error?error_:beat_;
        if(SDL_QueueAudio(device_,samples.data(),Uint32(samples.size()*sizeof(samples[0])))<0)
            SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,"Cannot play sound: %s",SDL_GetError());
    }
};
}
