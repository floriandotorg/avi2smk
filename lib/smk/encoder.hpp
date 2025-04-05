#pragma once

#include <cstdint>
#include <ostream>
#include <span>
#include <vector>

namespace smk {
    class encoder {
    public:
        explicit encoder(std::ostream &file, uint32_t width, uint32_t height, uint32_t fps, uint32_t num_frames);
        void encode_frame(const std::span<uint8_t> &frame);

    private:
        class bitstream {
        public:
            using value_type = uint16_t;

            bitstream(std::ostream &file);
            void write(value_type value, uint8_t length);
            void flush();

        private:
            std::ostream &_file;
            uint8_t _buf = 0;
            uint8_t _bits_in_buf = 0;
        };

        std::ostream &_file;
    };
}
