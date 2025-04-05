#include "encoder.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace smk {
    encoder::bitstream::bitstream(std::ostream &file) : _file(file) {}

    void encoder::bitstream::write(value_type value, uint8_t length) {
        if (std::numeric_limits<value_type>::digits < length) {
            throw std::invalid_argument("length exceeds value type");
        }

        while (length > 0) {
            const uint8_t space = 8 - _bits_in_buf;
            const uint8_t bits_to_write = std::min(length, space);
            const value_type mask = (static_cast<value_type>(1) << bits_to_write) - 1;
            _buf |= ((value & mask) << _bits_in_buf);
            _bits_in_buf += bits_to_write;
            if (_bits_in_buf == 8) {
                flush();
            }
            value >>= bits_to_write;
            length -= bits_to_write;
        }
    }

    void encoder::bitstream::flush() {
        if (_bits_in_buf > 0) {
            _file.write(reinterpret_cast<const char*>(&_buf), 1);
            _buf = 0;
            _bits_in_buf = 0;
        }
    }

    encoder::encoder(std::ostream &file, uint32_t width, uint32_t height, uint32_t fps, uint32_t num_frames) : _file(file) {
    }

    void encoder::encode_frame(const std::span<uint8_t> &frame) {

    }
}
