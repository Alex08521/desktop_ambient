#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

namespace ambient{
    static std::atomic<bool> gIsOurAudioPlaying{false};

    class AudioPlayer {
    public:
        AudioPlayer() = default;
        ~AudioPlayer();

        bool init();
        void play();
        void pause();
        void stop();
        bool isPlaying() const;

        void setVolume(double volume);
        double getVolume() const;

    private:
        void playbackThread();

        std::vector<uint8_t> vSoundData;
        uint32_t sample_rate = 44100;
        uint8_t channels = 2;
        uint8_t bits_per_sample = 16;
        
        std::atomic<bool> is_playing{false};
        std::atomic<bool> stop_requested{false};
        std::thread playback_thread;
        
        void* pa_stream = nullptr;
        double current_volume = 0.5;
    };
    
} //ambient