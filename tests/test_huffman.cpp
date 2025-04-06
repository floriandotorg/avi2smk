#include <string>
#include <sstream>
#include <vector>

#include "util.hpp"

#include <iostream>

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

void test_decode16() {
    const std::string text =  R"(Whereas recognition of the inherent dignity and of the equal and inalienable rights of all members of the human family is the foundation of freedom, justice and peace in the world,

Whereas disregard and contempt for human rights have resulted in barbarous acts which have outraged the conscience of mankind, and the advent of a world in which human beings shall enjoy freedom of speech and belief and freedom from fear and want has been proclaimed as the highest aspiration of the common people,

Whereas it is essential, if man is not to be compelled to have recourse, as a last resort, to rebellion against tyranny and oppression, that human rights should be protected by the rule of law,

Whereas it is essential to promote the development of friendly relations between nations,

Whereas the peoples of the United Nations have in the Charter reaffirmed their faith in fundamental human rights, in the dignity and worth of the human person and in the equal rights of men and women and have determined to promote social progress and better standards of life in larger freedom,

Whereas Member States have pledged themselves to achieve, in co-operation with the United Nations, the promotion of universal respect for and observance of human rights and fundamental freedoms,

Whereas a common understanding of these rights and freedoms is of the greatest importance for the full realization of this pledge,

Now, therefore,

The General Assembly,

Proclaims this Universal Declaration of Human Rights as a common standard of achievement for all peoples and all nations, to the end that every individual and every organ of society, keeping this Declaration constantly in mind, shall strive by teaching and education to promote respect for these rights and freedoms and by progressive measures, national and international, to secure their universal and effective recognition and observance, both among the peoples of Member States themselves and among the peoples of territories under their jurisdiction.)";

    std::vector<uint16_t> pairs;
    for (size_t n = 0; n < text.size(); n += 2) {
        pairs.push_back(static_cast<uint16_t>(text[n]) << 8 | static_cast<uint16_t>(text[n + 1]));
    }

    std::stringstream ss;
    auto bitstream = smk::encoder::bitstream(ss);

    auto huffman_tree = smk::encoder::huffman_tree<uint16_t>(bitstream);

    for (const auto &c : pairs) {
        huffman_tree.write(c);
    }

    huffman_tree.switch_to_write_mode();

    huffman_tree.pack();

    for (const auto &c : pairs) {
        huffman_tree.write(c);
    }

    bitstream.flush();

    smk::decoder decoder(ss, true);
    decoder._init_bitstream();
    auto huff16 = decoder._build_hoff16();

    for (const auto &c : pairs) {
        expect_eq(decoder._lookup_hoff16(huff16), c);
    }
}

int main() {
    test_pack();
    test_decode8();
    test_decode16();

    return 0;
}
