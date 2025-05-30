#include <string>
#include <sstream>
#include <cstddef>

#include "util.hpp"

int main() {
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

    smk::encoder::palette_type palette;
    for (size_t n = 0; n < 256; ++n) {
        const auto val = palmap[n % palmap.size()];
        palette[n] = { val, val, val };
    }

    std::stringstream ss;
    smk::encoder encoder(0, 0, 0);
    encoder._write_palette(ss, palette);

    smk::decoder decoder(ss, true);
    decoder._read_palette();

    expect_eq(decoder._palette, palette);

    return 0;
}
