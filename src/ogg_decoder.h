#pragma once

#include <vector>
#include <cstdint>
#include <string>


namespace ambient {

    class OggDecoder {
    public:
        OggDecoder();
        ~OggDecoder();
        
        bool decode(const uint8_t* data, size_t size);
        const std::vector<uint8_t>& getPcmData() const;
        uint32_t getSampleRate() const;
        uint8_t getChannels() const;
        uint8_t getBitsPerSample() const;
        const std::string& getLastError() const;
        
    private:
        std::vector<uint8_t> pcm_data;
        uint32_t sample_rate = 0;
        uint8_t channels = 0;
        uint8_t bits_per_sample = 16;
        std::string last_error;
    };

} //ambient