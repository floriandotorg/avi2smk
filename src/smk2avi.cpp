#include <iostream>

#include "avi/encoder.hpp"
#include "smk/decoder.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    smk::decoder decoder(file);

    std::ofstream output("output.avi", std::ios::binary);
    avi::encoder encoder(output, decoder.width(), decoder.height(), decoder.framerate(), decoder.num_frames());

    for (size_t n = 0; n < decoder.num_frames(); ++n) {
        std::cout << "Frame " << n + 1 << "... " << std::flush;
        encoder.encode_frame(decoder.decode_frame());
    }

    return 0;
}
