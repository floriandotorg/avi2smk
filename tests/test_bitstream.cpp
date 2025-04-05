#include <sstream>
#include <limits>

#include "util.hpp"

#define private public
#include "../lib/smk/encoder.hpp"
#undef private

#define private public: decoder(std::istream &file, bool testing_only) : _file(file) {}; public
#include "../lib/smk/decoder.hpp"
#undef private

int main() {
    std::stringstream ss;
    auto bitstream = smk::encoder::bitstream(ss);

    bitstream.write(0b1, 1);
    bitstream.write(0b1, 1);
    bitstream.write(0b1, 1);
    bitstream.write(0b0, 1);
    bitstream.write(0b0, 1);
    bitstream.write(0b1, 1);
    bitstream.flush();

    expect_eq(ss.str().size(), 1);

    {
        auto decoder = smk::decoder(ss, true);

        decoder._init_bitstream();

        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 0);
        expect_eq(decoder._bitstream_read_bit(), 0);
        expect_eq(decoder._bitstream_read_bit(), 1);
    }

    ss.str("");

    bitstream.write(0b10101010, 8);
    bitstream.write(0b00001111, 4);
    bitstream.write(0b1100110011001100, 12);
    bitstream.flush();

    expect_eq(ss.str().size(), 3);

    bitstream.write(0b00000011, 2);
    bitstream.flush();

    expect_eq(ss.str().size(), 4);

    {
        auto decoder = smk::decoder(ss, true);

        decoder._init_bitstream();

        expect_eq(decoder._bitstream_read_byte(), 0b10101010);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 0);
        expect_eq(decoder._bitstream_read_bit(), 0);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_bit(), 1);
        expect_eq(decoder._bitstream_read_byte(), 0b11001100);
        expect_eq(decoder._bitstream_read_byte(), 0b00000011);
    }

    expect_throw([&]() {
        bitstream.write(0, std::numeric_limits<smk::encoder::bitstream::value_type>::digits + 1);
    });

    return 0;
}
