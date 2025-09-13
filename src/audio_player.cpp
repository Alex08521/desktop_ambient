#include "audio_player.h"
#include "audio.h"
#include "ogg_decoder.h"

#include <iostream>

namespace ambient{

    AudioPlayer::~AudioPlayer() {
        stop();
    }

    bool validateOggData(const uint8_t* data, size_t size) {
        if (size < 4) return false;
        if (data[0] != 'O' || data[1] != 'g' || data[2] != 'g' || data[3] != 'S') {
            std::cerr << "Invalid OGG signature" << std::endl;
            return false;
        }
        return true;
    }

    bool AudioPlayer::init() {
        std::cout << "Player start init\n";

        if (!validateOggData(audio_data, audio_size)) {
            std::cerr << "Invalid OGG data" << std::endl;
            return false;
        }

        OggDecoder decoder;
        std::cout << "\tPlayer starting decoding audio\n";
        if (!decoder.decode(audio_data, audio_size)) {
            std::cerr << "Failed to decode Ogg Vorbis data" << std::endl;
            return false;
        }
        std::cout << "\tPlayer finishing decoding audio\n";
        
        vSoundData = decoder.getPcmData();
        sample_rate = decoder.getSampleRate();
        channels = decoder.getChannels();
        bits_per_sample = decoder.getBitsPerSample();
        
        if (vSoundData.empty()) {
            std::cerr << "No sound data available after decoding" << std::endl;
            return false;
        }
        
        std::cout << "Decoded audio: " << audio_size << " bytes, "
                  << sample_rate << " Hz, " << (int)channels << " channels, "
                  << (int)bits_per_sample << " bits per sample" 
                  << "\nPlayer finish init"<< std::endl;
                  
        return true;
    }

    void AudioPlayer::play() {
        if (is_playing) return;
    
        std::cout << "Player start playing audio\n";
        stop_requested = false;
        is_playing = true;
        gIsOurAudioPlaying = true;
        
        if (!playback_thread.joinable()) {
            playback_thread = std::thread(&AudioPlayer::playbackThread, this);
        }
    }

    void AudioPlayer::pause() {
        std::cout << "Player paused played audio\n";
        is_playing = false;
        gIsOurAudioPlaying = false;
    }

    void AudioPlayer::stop() {
        std::cout << "Player stopping\n";
        stop_requested = true;
        is_playing = false;
        gIsOurAudioPlaying = false;
        
        if (playback_thread.joinable()) {
            playback_thread.join();
        }
        
        if (pa_stream) {
            pa_simple_free(reinterpret_cast<pa_simple*>(pa_stream));
            pa_stream = nullptr;
        }
    }

    bool AudioPlayer::isPlaying() const {
        return is_playing;
    }

    void AudioPlayer::playbackThread() {
        pa_sample_spec ss;
        
        ss.format = (bits_per_sample == 8) ? PA_SAMPLE_U8 : 
                    (bits_per_sample == 16) ? PA_SAMPLE_S16LE :
                    (bits_per_sample == 32) ? PA_SAMPLE_S32LE : PA_SAMPLE_S16LE;
        
        ss.rate = sample_rate;
        ss.channels = channels;
        
        int error;
        pa_simple* s = pa_simple_new(
            nullptr,
            "desktop_ambient",
            PA_STREAM_PLAYBACK,
            nullptr,
            "BackgroundSound",
            &ss,
            nullptr,
            nullptr,
            &error
        );
        
        if (!s) {
            std::cerr << "Failed to create PulseAudio stream: " << pa_strerror(error) << std::endl;
            is_playing = false;
            return;
        }
        
        pa_stream = s;
        size_t offset = 0;
        
        while (!stop_requested) {
           if (!is_playing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            size_t remaining = vSoundData.size() - offset;
            size_t to_write = std::min(remaining, static_cast<size_t>(4096));
            
            if (to_write > 0) {
                if (pa_simple_write(s, vSoundData.data() + offset, to_write, &error) < 0) {
                    std::cerr << "Failed to write to PulseAudio: " << pa_strerror(error) << std::endl;
                    break;
                }
                offset += to_write;
            }
            
            if (offset >= vSoundData.size()) {
                offset = 0;
            }
        }
        
        if (pa_simple_drain(s, &error) < 0) {
            std::cerr << "Failed to drain PulseAudio: " << pa_strerror(error) << std::endl;
        }
        
        pa_simple_free(s);
        pa_stream = nullptr;
        is_playing = false;
    }

    void AudioPlayer::setVolume(double volume) {
        current_volume = std::max(0.0, std::min(1.0, volume));
    }

    double AudioPlayer::getVolume() const {
        return current_volume;
    }

} //ambient