#pragma once

#include <cstdint>
#include <cstddef>
#include <istream>
#include <span>
#include <vector>

namespace avi {
    class decoder {
    public:
        explicit decoder(std::istream &file);
        std::span<uint8_t> decode_frame();

        size_t height() const { return _height; }
        size_t width() const { return _width; }
        size_t num_frames() const { return _num_frames; }
        size_t fps() const { return _fps; }

    private:
        std::istream &_file;
        size_t _height;
        size_t _width;
        size_t _num_frames;
        size_t _fps;

        std::vector<uint8_t> _frame;
    };
}
