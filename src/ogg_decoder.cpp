// ogg_decoder.cpp
#include "ogg_decoder.h"
#include <vorbis/vorbisfile.h>
#include <iostream>
#include <cstring>
#include <sstream>

namespace ambient {

    struct OggMemoryFile {
        const uint8_t* data;
        size_t size;
        size_t position;
    };

    static size_t read_func(void* ptr, size_t size, size_t nmemb, void* datasource) {
        OggMemoryFile* mf = static_cast<OggMemoryFile*>(datasource);
        size_t bytes_to_read = std::min(size * nmemb, mf->size - mf->position);
        
        if (bytes_to_read > 0) {
            memcpy(ptr, mf->data + mf->position, bytes_to_read);
            mf->position += bytes_to_read;
        }
        
        return bytes_to_read;
    }

    static int seek_func(void* datasource, ogg_int64_t offset, int whence) {
        OggMemoryFile* mf = static_cast<OggMemoryFile*>(datasource);
        size_t new_position;
        
        switch (whence) {
            case SEEK_SET:
                new_position = offset;
                break;
            case SEEK_CUR:
                new_position = mf->position + offset;
                break;
            case SEEK_END:
                new_position = mf->size + offset;
                break;
            default:
                return -1;
        }
        
        if (new_position > mf->size) {
            return -1;
        }
        
        mf->position = new_position;
        return 0;
    }

    static int close_func(void* /*datasource*/) {
        return 0;
    }

    static long tell_func(void* datasource) {
        OggMemoryFile* mf = static_cast<OggMemoryFile*>(datasource);
        return mf->position;
    }

    OggDecoder::OggDecoder() = default;

    OggDecoder::~OggDecoder() = default;

    bool OggDecoder::decode(const uint8_t* data, size_t size) {
        std::cout << "Decode staring\n";
        pcm_data.clear();
        last_error.clear();
        
        if (data == nullptr || size == 0) {
            last_error = "No data provided";
            return false;
        }
        
        if (size < 4 || data[0] != 'O' || data[1] != 'g' || data[2] != 'g' || data[3] != 'S') {
            last_error = "Invalid OGG signature";
            return false;
        }
        
        OggMemoryFile mf = {data, size, 0};
        
        OggVorbis_File vf;
        ov_callbacks callbacks;
        callbacks.read_func = read_func;
        callbacks.seek_func = seek_func;
        callbacks.close_func = close_func;
        callbacks.tell_func = tell_func;
        
        int result = ov_open_callbacks(&mf, &vf, nullptr, 0, callbacks);
        if (result != 0) {
            std::stringstream ss;
            ss << "ov_open_callbacks failed with error: " << result;
            last_error = ss.str();
            return false;
        }
        
        vorbis_info* vi = ov_info(&vf, -1);
        if (!vi) {
            last_error = "ov_info failed";
            ov_clear(&vf);
            return false;
        }
        
        sample_rate = vi->rate;
        channels = vi->channels;
        bits_per_sample = 16;
        
        const int buffer_size = 4096;
        char pcm_buffer[buffer_size];
        int current_section;
        long read_result;
        
        while ((read_result = ov_read(&vf, pcm_buffer, buffer_size, 0, 2, 1, &current_section)) > 0) {
            pcm_data.insert(pcm_data.end(), pcm_buffer, pcm_buffer + read_result);
        }
        
        if (read_result < 0) {
            std::stringstream ss;
            ss << "ov_read failed with error: " << read_result;
            last_error = ss.str();
            ov_clear(&vf);
            return false;
        }
        
        ov_clear(&vf);
        
        if (pcm_data.empty()) {
            last_error = "No PCM data decoded";
            return false;
        }
        
        return true;
    }

    const std::vector<uint8_t>& OggDecoder::getPcmData() const {
        return pcm_data;
    }

    uint32_t OggDecoder::getSampleRate() const {
        return sample_rate;
    }

    uint8_t OggDecoder::getChannels() const {
        return channels;
    }

    uint8_t OggDecoder::getBitsPerSample() const {
        return bits_per_sample;
    }

    const std::string& OggDecoder::getLastError() const {
        return last_error;
    }

} // namespace ambient