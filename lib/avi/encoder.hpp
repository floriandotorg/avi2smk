#pragma once

#include <cstdint>
#include <ostream>
#include <span>
#include <vector>

namespace avi {
    class encoder {
    public:
        explicit encoder(std::ostream &file, uint32_t width, uint32_t height, uint32_t fps, uint32_t num_frames);
        ~encoder();
        void encode_frame(const std::span<uint8_t> &frame);

    private:
        std::ostream &_file;
        uint32_t _width;
        std::vector<uint8_t> _pad;
        uint32_t _total_frame_size;
    };
}
