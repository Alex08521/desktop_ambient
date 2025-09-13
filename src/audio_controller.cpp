#include "audio_controller.h"
#include <string.h>
#include <pulse/glib-mainloop.h>
#include <pulse/volume.h>
#include <pulse/ext-stream-restore.h>
#include <pulse/error.h>
#include <cmath>
#include <algorithm>

namespace ambient{

    AudioController::AudioController() : last_activity_time(std::chrono::steady_clock::now()) {
        if(!init()) throw std::runtime_error("Audio controller not inited");
    };

    AudioController::~AudioController() {
        stop();
    }

    bool AudioController::init() {
        return player.init();
    }

    void AudioController::start() {
        if (running) return;
        
        running = true;
        player.play();
        monitor_thread = std::thread(&AudioController::monitorAudioActivity, this);
        system_monitor_thread = std::thread(&AudioController::monitorSystemOutput, this);
    }

    void AudioController::stop() {
        running = false;
        player.stop();
        
        if (monitor_thread.joinable()) {
            monitor_thread.join();
        }
        
        if (system_monitor_thread.joinable()) {
            system_monitor_thread.join();
        }
        
        if (monitor_stream) {
            pa_stream_disconnect(monitor_stream);
            pa_stream_unref(monitor_stream);
            monitor_stream = nullptr;
        }
        
        if (monitor_context) {
            pa_context_disconnect(monitor_context);
            pa_context_unref(monitor_context);
            monitor_context = nullptr;
        }
        
        if (monitor_mainloop) {
            pa_mainloop_free(monitor_mainloop);
            monitor_mainloop = nullptr;
        }
    }

    void AudioController::contextStateCallback(pa_context* c, void* userdata) {
        auto* controller = static_cast<AudioController*>(userdata);
        
        switch (pa_context_get_state(c)) {
            case PA_CONTEXT_READY:
                pa_context_subscribe(c, static_cast<pa_subscription_mask_t>(
                    PA_SUBSCRIPTION_MASK_SINK_INPUT | 
                    PA_SUBSCRIPTION_MASK_SINK), 
                    nullptr, nullptr);
                pa_context_get_sink_info_list(c, sinkInfoCallback, userdata);
                break;
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                controller->stop();
                break;
            default:
                break;
        }
    }

    void AudioController::subscribeCallback(pa_context* c, pa_subscription_event_type_t t, uint32_t idx, void* userdata) {
        switch (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
            case PA_SUBSCRIPTION_EVENT_NEW:
            case PA_SUBSCRIPTION_EVENT_CHANGE:
                if (t & PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
                    pa_context_get_sink_input_info(c, idx, sinkInputInfoCallback, userdata);
                } else if (t & PA_SUBSCRIPTION_EVENT_SINK) {
                    pa_context_get_sink_info_by_index(c, idx, sinkInfoCallback, userdata);
                }
                break;
        }
    }

    void AudioController::sinkInputInfoCallback([[maybe_unused]]pa_context* c, [[maybe_unused]]const pa_sink_input_info* i, [[maybe_unused]]int eol, [[maybe_unused]]void* userdata) {
        return;
    }

    void AudioController::sinkInfoCallback([[maybe_unused]]pa_context* c, const pa_sink_info* i, int eol, void* userdata) {
        auto* controller = static_cast<AudioController*>(userdata);
        
        if (eol) {
            return;
        }
        
        if (i->monitor_source_name && (i->flags & PA_SINK_HW_VOLUME_CTRL)) {
            controller->setupMonitorStream();
        }
    }

    void AudioController::streamReadCallback(pa_stream* s, size_t length, void* userdata) {
        auto* controller = static_cast<AudioController*>(userdata);
        const void* data;
        
        if (gIsOurAudioPlaying) {
            pa_stream_drop(s);
            controller->current_system_volume = 0.0;
            return;
        }
        
        if (pa_stream_peek(s, &data, &length) < 0) {
            std::cerr << "Failed to read data from monitor stream" << std::endl;
            return;
        }
        
        if (length > 0 && data) {
            double volume = 0.0;
            
            const int16_t* samples = static_cast<const int16_t*>(data);
            size_t sample_count = length / sizeof(int16_t) / 2;
            
            for (size_t i = 0; i < sample_count * 2; i += 2) {
                double left = samples[i] / 32768.0;
                double right = samples[i + 1] / 32768.0;
                volume += (left * left + right * right) / 4.0;
            }
            
            volume = std::sqrt(volume / sample_count);
            
            controller->volume_history.push_back(volume);
            if (controller->volume_history.size() > HISTORY_SIZE) {
                controller->volume_history.pop_front();
            }
            
            double avg_volume = 0.0;
            for (double v : controller->volume_history) {
                avg_volume += v;
            }
            avg_volume /= controller->volume_history.size();
            
            controller->current_system_volume = avg_volume;
        }
        
        pa_stream_drop(s);
    }

    void AudioController::streamStateCallback(pa_stream* s, [[maybe_unused]]void* userdata) {      
        switch (pa_stream_get_state(s)) {
            case PA_STREAM_READY:
                std::cout << "Monitor stream ready" << std::endl;
                break;
            case PA_STREAM_FAILED:
                std::cerr << "Monitor stream failed" << std::endl;
                break;
            case PA_STREAM_TERMINATED:
                std::cout << "Monitor stream terminated" << std::endl;
                break;
            default:
                break;
        }
    }

    bool AudioController::setupMonitorStream() {
        pa_operation* op = pa_context_get_sink_info_by_name(monitor_context, 
                                                          "@DEFAULT_SINK@", 
                                                          [](pa_context* c, const pa_sink_info* i, int eol, void* userdata) {
            auto* controller = static_cast<AudioController*>(userdata);
            
            if (eol || !i) {
                return;
            }
            
            pa_sample_spec ss;
            ss.format = PA_SAMPLE_S16LE;
            ss.rate = 44100;
            ss.channels = 2;
            
            pa_buffer_attr attr;
            attr.maxlength = static_cast<uint32_t>(-1);
            attr.tlength = static_cast<uint32_t>(-1);
            attr.prebuf = static_cast<uint32_t>(-1);
            attr.minreq = static_cast<uint32_t>(-1);
            attr.fragsize = static_cast<uint32_t>(-1);
            
            controller->monitor_stream = pa_stream_new(c, "System Output Monitor", &ss, nullptr);
            pa_stream_set_state_callback(controller->monitor_stream, streamStateCallback, controller);
            pa_stream_set_read_callback(controller->monitor_stream, streamReadCallback, controller);
            
            if (pa_stream_connect_record(controller->monitor_stream, i->monitor_source_name, &attr, 
                                        static_cast<pa_stream_flags_t>(PA_STREAM_PEAK_DETECT | PA_STREAM_ADJUST_LATENCY)) < 0) {
                std::cerr << "Failed to connect monitor stream: " << pa_strerror(pa_context_errno(c)) << std::endl;
                return;
            }
        }, this);
        
        pa_operation_unref(op);
        return true;
    }

    void AudioController::updateAudioActivity(double system_volume) {
        bool is_active = system_volume > VOLUME_THRESHOLD;
        
        if (is_active) {
            last_activity_time = std::chrono::steady_clock::now();
            system_was_active = true;
            
            if (player.isPlaying()) {
                std::cout << "System audio active (" << system_volume << "), pausing playback" << std::endl;
                player.pause();
            }
        } else if (system_was_active) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_time).count();
            
            if (elapsed >= RESUME_DELAY_MS) {
                if (!player.isPlaying()) {
                    std::cout << "System audio inactive for " << elapsed << "ms (" << system_volume << "), resuming playback" << std::endl;
                    player.play();
                }
                system_was_active = false;
            }
        } else {
            if (!player.isPlaying()) {
                std::cout << "System audio inactive (" << system_volume << "), resuming playback" << std::endl;
                player.play();
            }
        }
    }

    void AudioController::monitorAudioActivity() {
        pa_glib_mainloop* mainloop = pa_glib_mainloop_new(nullptr);
        if (!mainloop) {
            std::cerr << "Failed to create PulseAudio mainloop" << std::endl;
            return;
        }
        
        pa_context* context = pa_context_new(pa_glib_mainloop_get_api(mainloop), "desktop_ambient_monitor");
        if (!context) {
            std::cerr << "Failed to create PulseAudio context" << std::endl;
            pa_glib_mainloop_free(mainloop);
            return;
        }
        
        pa_context_set_state_callback(context, contextStateCallback, this);
        pa_context_set_subscribe_callback(context, subscribeCallback, this);
        
        if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
            std::cerr << "Failed to connect PulseAudio context" << std::endl;
            pa_context_unref(context);
            pa_glib_mainloop_free(mainloop);
            return;
        }
        
        GMainContext* glib_context = g_main_context_new();
        GMainLoop* loop = g_main_loop_new(glib_context, FALSE);
        
        while (running) {
            g_main_context_iteration(glib_context, FALSE);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        g_main_loop_unref(loop);
        pa_context_unref(context);
        pa_glib_mainloop_free(mainloop);
    }

    void AudioController::monitorSystemOutput() {
        monitor_mainloop = pa_mainloop_new();
        if (!monitor_mainloop) {
            std::cerr << "Failed to create monitor mainloop" << std::endl;
            return;
        }
        
        monitor_context = pa_context_new(pa_mainloop_get_api(monitor_mainloop), "system_output_monitor");
        if (!monitor_context) {
            std::cerr << "Failed to create monitor context" << std::endl;
            pa_mainloop_free(monitor_mainloop);
            return;
        }
        
        pa_context_set_state_callback(monitor_context, [](pa_context* c, void* userdata) {
            auto* controller = static_cast<AudioController*>(userdata);
            
            switch (pa_context_get_state(c)) {
                case PA_CONTEXT_READY:
                    controller->setupMonitorStream();
                    break;
                case PA_CONTEXT_FAILED:
                case PA_CONTEXT_TERMINATED:
                    std::cerr << "Monitor context failed or terminated" << std::endl;
                    break;
                default:
                    break;
            }
        }, this);
        
        if (pa_context_connect(monitor_context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
            std::cerr << "Failed to connect monitor context" << std::endl;
            pa_context_unref(monitor_context);
            pa_mainloop_free(monitor_mainloop);
            return;
        }
        
        while (running) {
            pa_mainloop_iterate(monitor_mainloop, 0, nullptr);
            updateAudioActivity(current_system_volume);
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
        }
    }

    bool AudioController::checkIfOurAppIsPlaying() {
        bool is_our_app = false;
    
        pa_mainloop* temp_ml = pa_mainloop_new();
        pa_mainloop_api* temp_mlapi = pa_mainloop_get_api(temp_ml);
        pa_context* temp_ctx = pa_context_new(temp_mlapi, "temp_check");
        
        pa_context_connect(temp_ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
        
        pa_context_state_t state;
        do {
            pa_mainloop_iterate(temp_ml, 1, nullptr);
            state = pa_context_get_state(temp_ctx);
        } while (state != PA_CONTEXT_READY && state != PA_CONTEXT_FAILED);
        
        if (state == PA_CONTEXT_READY) {
            pa_operation* op = pa_context_get_sink_input_info_list(temp_ctx,
                []([[maybe_unused]]pa_context* c, const pa_sink_input_info* i, int eol, void* userdata) {
                    auto* is_our_app_ptr = static_cast<bool*>(userdata);
                    
                    if (eol) {
                        return;
                    }
                    
                    if (i && i->proplist) {
                        const char* app_name = pa_proplist_gets(i->proplist, "application.name");
                        const char* process_binary = pa_proplist_gets(i->proplist, "application.process.binary");
                        
                        if (app_name && strstr(app_name, "desktop_ambient") != nullptr) {
                            *is_our_app_ptr = true;
                        } else if (process_binary && (
                            strstr(process_binary, "desktop_ambient") != nullptr ||
                            strstr(process_binary, "audio_controller") != nullptr)) {
                            *is_our_app_ptr = true;
                        }
                    }
                }, 
                &is_our_app
            );
            
            if (op) {
                while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
                    pa_mainloop_iterate(temp_ml, 1, nullptr);
                }
                pa_operation_unref(op);
            }
        }
        
        pa_context_disconnect(temp_ctx);
        pa_context_unref(temp_ctx);
        pa_mainloop_free(temp_ml);
        
        return is_our_app;
    }

} //ambient