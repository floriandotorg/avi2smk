#include <sstream>
#include <limits>

#include "util.hpp"

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

    bitstream.write(0b1, 3);
    bitstream.write(0b1, 1);
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
        expect_eq(decoder._bitstream_read_byte(), 0b00001001);
    }

    expect_throw([&]() {
        bitstream.write(0, std::numeric_limits<smk::encoder::bitstream::value_type>::digits + 1);
    });

    return 0;
}
