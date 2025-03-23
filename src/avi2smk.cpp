#include <fstream>
#include <iostream>
#include <format>

#include "avi/decoder.hpp"
#include "avi/encoder.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << std::format("Usage: {} <input file>", argv[0]) << std::endl;
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    avi::decoder decoder(file);

    std::ofstream output("output.avi", std::ios::binary);
    avi::encoder encoder(output, decoder.width(), decoder.height(), decoder.fps(), decoder.num_frames());

    for (size_t n = 0; n < decoder.num_frames(); ++n) {
        std::cout << std::format("Frame {}... ", n + 1) << std::flush;
        encoder.encode_frame(decoder.decode_frame());
    }

    return 0;
}
