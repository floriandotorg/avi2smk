#include "encoder.hpp"

#include <algorithm>
#include <stdexcept>
#include <limits>

namespace smk {
    encoder::bitstream::bitstream(std::ostream &file) : _file(file) {}

    void encoder::bitstream::write(value_type value, uint8_t length) {
        if (length > std::numeric_limits<value_type>::digits) {
            throw std::invalid_argument("length exceeds value type");
        }

        while (length > 0) {
            const auto bits_to_write = std::min<uint8_t>(length, 8 - _bits_in_buf);
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

    void encoder::_write_palette(const palette_type &palette) {
        constexpr std::array<uint8_t, 64> palmap = {
            0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
            0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
            0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D,
            0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
            0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E,
            0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
            0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF,
            0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
        };

        const auto get_index = [&](const uint8_t val) {
            static_assert(palmap.size() <= std::numeric_limits<uint8_t>::max());

            for (uint8_t n = 0; n < palmap.size(); ++n) {
                if (palmap[n] >= val) {
                    return n;
                }
            }

            throw std::runtime_error("color exceeds palmap");
        };

        _file.put(static_cast<char>(192)); // length of palette (256 * 3 / 4)

        for (const auto &color : palette) {
            _file.put(get_index(color[0]));
            _file.put(get_index(color[1]));
            _file.put(get_index(color[2]));
        }
    }
}
