#include "encoder.hpp"

#include <bit>

template<typename T>
void write(std::ostream &file, T value) {
    if constexpr (std::endian::native != std::endian::little) {
        value = std::byteswap(value);
    }
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

namespace avi {
    encoder::encoder(std::ostream &file, uint32_t width, uint32_t height, uint32_t fps, uint32_t num_frames)
    : _file(file), _width(width), _pad(((4 - (width * 3) % 4) % 4)) {
        std::fill(_pad.begin(), _pad.end(), 0);
        _total_frame_size = (width + _pad.size()) * height * 3;

        _file.write("RIFF", 4);
        write<uint32_t>(_file, 0);
        _file.write("AVI ", 4);
        _file.write("LIST", 4);
        write<uint32_t>(_file,  4 + 64 + 124); // size of LIST chunk
        _file.write("hdrl", 4);
        _file.write("avih", 4);
        write<uint32_t>(_file, 56); // size of avih chunk
        write<uint32_t>(_file, 1000000 / fps); // microseconds per frame
        write<uint32_t>(_file, _total_frame_size); // max bytes per second
        write<uint32_t>(_file, 1); // padding granule
        write<uint32_t>(_file, 0); // flags
        write<uint32_t>(_file, num_frames); // total frames
        write<uint32_t>(_file, 0); // initial frames
        write<uint32_t>(_file, 1); // number of streams
        write<uint32_t>(_file, _total_frame_size); // suggested buffer size
        write(_file, width); // width
        write(_file, height); // height
        file.write("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);
        _file.write("LIST", 4);
        write<uint32_t>(_file, 116); // size of LIST chunk
        _file.write("strl", 4);
        _file.write("strh", 4);
        write<uint32_t>(_file, 56); // size of strh chunk
        _file.write("vids", 4);
        _file.write("DIB ", 4);
        write<uint32_t>(_file, 0); // flags
        write<uint16_t>(_file, 0); // priority
        write<uint16_t>(_file, 0); // language
        write<uint32_t>(_file, 0); // initial frames
        write<uint32_t>(_file, 1); // scale
        write<uint32_t>(_file, fps); // rate
        write<uint32_t>(_file, 0); // start
        write<uint32_t>(_file, num_frames); // length
        write<uint32_t>(_file, _total_frame_size); // suggested buffer size
        write<uint32_t>(_file, 0); // quality
        write<uint32_t>(_file, _total_frame_size); // sample size
        write<uint32_t>(_file, 0); // rcFrame
        write<uint32_t>(_file, 0); // rcFrame: right, bottom
        _file.write("strf", 4);
        write<uint32_t>(_file, 40);
        write<uint32_t>(_file, 40);
        write<uint32_t>(_file, width);
        write<int32_t>(_file, static_cast<int32_t>(height) * -1);
        write<uint16_t>(_file, 1); // planes
        write<uint16_t>(_file, 24); // bit count
        write<uint32_t>(_file, 0); // no compression
        write<uint32_t>(_file, _total_frame_size); // size image
        write<uint32_t>(_file, 0); // x pels
        write<uint32_t>(_file, 0); // y pels
        write<uint32_t>(_file, 0); // colors used
        write<uint32_t>(_file, 0); // important colors
        _file.write("LIST", 4);
        write<uint32_t>(_file, num_frames * (_total_frame_size + 8) + 4); // size of LIST chunk
        _file.write("movi", 4);
    }

    encoder::~encoder() {
        const uint32_t size = _file.tellp();
        _file.seekp(4);
        write(_file, size - 8);
    }

    void encoder::encode_frame(const std::span<uint8_t> &frame) {
        _file.write("00db", 4);
        write(_file, _total_frame_size);
        for (size_t n = 0; n < frame.size() / 3; ++n) {
            write(_file, frame[n * 3 + 2]);
            write(_file, frame[n * 3 + 1]);
            write(_file, frame[n * 3]);

            if (n % (_width * 3) == 0) {
                _file.write(reinterpret_cast<const char*>(_pad.data()), _pad.size());
            }
        }
    }
}
