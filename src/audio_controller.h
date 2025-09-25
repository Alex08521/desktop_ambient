#pragma once

#include "audio_player.h"
#include <atomic>
#include <thread>
#include <iostream>
#include <unordered_map>
#include <deque>

namespace ambient{

    class AudioController {
    public:
        AudioController();
        ~AudioController();
        
        bool init();
        void start();
        void stop();
        
    private:
        void monitorAudioActivity();
        void monitorSystemOutput();
        void updateAudioActivity(double system_volume);
        bool setupMonitorStream();
        bool checkIfOurAppIsPlaying();

        static void contextStateCallback(pa_context* c, void* userdata);
        static void subscribeCallback(pa_context* c, pa_subscription_event_type_t t, uint32_t idx, void* userdata);
        static void sinkInputInfoCallback(pa_context* c, const pa_sink_input_info* i, int eol, void* userdata);
        static void sinkInfoCallback(pa_context* c, const pa_sink_info* i, int eol, void* userdata);
        static void streamReadCallback(pa_stream* s, size_t length, void* userdata);
        static void streamStateCallback(pa_stream* s, void* userdata);
        
        AudioPlayer player;
        std::atomic<bool> running{false};
        std::thread monitor_thread;
        std::thread system_monitor_thread;
        
        pa_stream* monitor_stream = nullptr;
        pa_context* monitor_context = nullptr;
        pa_mainloop* monitor_mainloop = nullptr;
        
        std::deque<double> volume_history;
        std::atomic<double> current_system_volume{0.0};
        
        static constexpr double VOLUME_THRESHOLD = 0.016;
        static constexpr double SILENCE_THRESHOLD = 0.001;
        static constexpr int HISTORY_SIZE = 60;
        static constexpr int CHECK_INTERVAL_MS = 50;
        static constexpr int RESUME_DELAY_MS = 1000;

        std::chrono::steady_clock::time_point last_activity_time;
        bool system_was_active = false;
    };

} //ambient