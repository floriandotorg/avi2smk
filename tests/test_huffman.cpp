#include <string>
#include <sstream>

#include "util.hpp"

void test_pack() {
    std::stringstream ss;
    auto bitstream = smk::encoder::bitstream(ss);
    auto tree = smk::encoder::huffman_tree<uint8_t>(bitstream);

    tree._root = std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{
        std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{
            std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{
                std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{ nullptr, nullptr, 3, 0 }),
                std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{
                    std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{ nullptr, nullptr, 4, 0 }),
                    std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{ nullptr, nullptr, 5, 0 }),
                    {},
                    0
                }),
                {},
                0
            }),
            std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{ nullptr, nullptr, 6, 0 }),
            {},
            0
        }),
        std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{
            std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{ nullptr, nullptr, 7, 0 }),
            std::make_unique<smk::encoder::huffman_tree<uint8_t>::node>(smk::encoder::huffman_tree<uint8_t>::node{ nullptr, nullptr, 8, 0 }),
            {},
            0
        }),
        {},
        0
    });

    tree.pack();
    bitstream.flush();

    std::stringstream result;
    smk::decoder decoder(ss, true);
    decoder._init_bitstream();
    for (size_t n = 0; n < 12; ++n) {
        const auto bit = decoder._bitstream_read_bit();
        if (bit == 0) {
            result << "0 " << static_cast<unsigned int>(decoder._bitstream_read_byte()) << " ";
        } else {
            result << "1 ";
        }
    }

    expect_eq(result.str(), "1 1 1 1 0 3 1 0 4 0 5 0 6 1 0 7 0 8 ");
    expect_eq(decoder._bitstream_read_bit(), 0);
}

void test_decode8() {
    const std::string text = "Everyone is entitled to all the rights and freedoms set forth in this Declaration, without distinction of any kind, such as race, colour, sex, language, religion, political or other opinion, national or social origin, property, birth or other status. Furthermore, no distinction shall be made on the basis of the political, jurisdictional or international status of the country or territory to which a person belongs, whether it be independent, trust, non-self-governing or under any other limitation of sovereignty.";

    std::stringstream ss;
    auto bitstream = smk::encoder::bitstream(ss);

    auto huffman_tree = smk::encoder::huffman_tree<char>(bitstream);

    for (const auto &c : text) {
        huffman_tree.write(c);
    }

    huffman_tree.switch_to_write_mode();

    huffman_tree.pack();

    for (const auto &c : text) {
        huffman_tree.write(c);
    }

    bitstream.flush();

    smk::decoder decoder(ss, true);
    decoder._init_bitstream();
    const auto huff8 = decoder._build_hoff8();

    for (const auto &c : text) {
        expect_eq(decoder._lookup_hoff8(huff8), c);
    }
}

int main() {
    test_pack();
    test_decode8();

    return 0;
}
