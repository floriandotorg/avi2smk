#include "encoder.hpp"

#include <algorithm>
#include <stdexcept>
#include <limits>
#include <format>
#include <bit>
#include <sstream>
#include <cassert>

template<typename T>
void write_le(std::ostream &file, T value) {
    if constexpr (std::endian::native != std::endian::little) {
        value = std::byteswap(value);
    }
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

namespace smk {
    encoder::bitstream::bitstream(std::ostream &file) : _file(file) {}

    void encoder::bitstream::write(value_type value, uint8_t length) {
        if (length > std::numeric_limits<value_type>::digits) {
            throw std::invalid_argument("Length exceeds value type");
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

    encoder::encoder(uint32_t width, uint32_t height, uint32_t fps) : _width(width), _height(height), _fps(fps) {
        if (width % 4 != 0 || height % 4 != 0) {
            throw std::invalid_argument("Width and height must be divisible by 4");
        }
    }

    void encoder::encode_frame(const std::span<uint8_t> &frame) {
        if (frame.size() != _width * _height * 3) {
            throw std::invalid_argument("Frame data does not match width and height");
        }

        _frames.emplace_back(std::move(std::vector<uint8_t>(frame.begin(), frame.end())));
    }

    void encoder::write(std::ostream &file) {
        file.write("SMK2", 4);

        write_le<uint32_t>(file, _width);
        write_le<uint32_t>(file, _height);
        write_le<uint32_t>(file, _frames.size());
        write_le<uint32_t>(file, 1000 / _fps);

        write_le<uint32_t>(file, 0); // flags

        for (size_t n = 0; n < 7; ++n) {
            write_le<uint32_t>(file, 0); // audio size
        }

        std::ostringstream ss(std::ios::binary);
        bitstream bs(ss);
        huffman_tree<uint16_t> type(bs);
        huffman_tree<uint16_t> mmap(bs);
        huffman_tree<uint16_t> mclr(bs);
        huffman_tree<uint16_t> full(bs);

        palette_type palette;
        size_t color_count = 0;
        for (const auto &frame : _frames) {
            for (size_t n = 0; n < frame.size(); n += 3) {
                bool found = false;

                for (size_t m = 0; m < color_count; ++m) {
                    if (palette[m][0] == frame[n] && palette[m][1] == frame[n + 1] && palette[m][2] == frame[n + 2]) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    palette[color_count++] = { frame[n], frame[n + 1], frame[n + 2] };

                    if (color_count >= 256) {
                        throw std::runtime_error("Too many colors");
                    }
                }
            }
        }

        const auto get_color_index = [&](uint8_t r, uint8_t g, uint8_t b) {
            for (size_t n = 0; n < color_count; ++n) {
                if (palette[n][0] == r && palette[n][1] == g && palette[n][2] == b) {
                    return n;
                }
            }

            throw std::runtime_error("Color not found");
        };

        struct chain {
            block_type type;
            size_t length;
            uint8_t data;
            std::vector<block> blocks;
        };

        std::vector<std::vector<chain>> frame_chains;
        std::vector<uint8_t> last_frame(_width * _height * 3);
        for (size_t current_frame_index = 0; current_frame_index < _frames.size(); ++current_frame_index) {
            const auto &frame = _frames[current_frame_index];

            struct preprocessed_block {
                block_type type;
                block data;
            };

            std::vector<preprocessed_block> blocks;
            for (size_t y = 0; y < _height; y += 4) {
                for (size_t x = 0; x < _width; x += 4) {
                    std::vector<std::array<uint8_t, 3>> colors;
                    colors.reserve(3);
                    bool same_as_last = current_frame_index > 0;
                    for (size_t y_off = 0; y_off < 4; ++y_off) {
                        for (size_t x_off = 0; x_off < 4; ++x_off) {
                            const size_t p = (y + y_off) * _width * 3 + (x + x_off) * 3;
                            if (same_as_last && (frame[p] != last_frame[p] || frame[p + 1] != last_frame[p + 1] || frame[p + 2] != last_frame[p + 2])) {
                                same_as_last = false;
                            }

                            const auto color = std::array{ frame[p], frame[p + 1], frame[p + 2] };
                            if (colors.size() < 3 && !std::ranges::contains(colors, color)) {
                                colors.emplace_back(color);
                            }
                        }
                    }

                    assert(colors.size() > 0);

                    if (same_as_last) {
                        blocks.emplace_back(preprocessed_block{ block_type::void_, {} });
                        continue;
                    }

                    if (colors.size() < 2) {
                        block block;
                        block.solid.color = get_color_index(colors[0][0], colors[0][1], colors[0][2]);
                        blocks.emplace_back(preprocessed_block{ block_type::solid, block });
                    } else if (colors.size() == 2) {
                        const uint8_t idx_color1 = get_color_index(colors[0][0], colors[0][1], colors[0][2]);
                        const uint8_t idx_color0 = get_color_index(colors[1][0], colors[1][1], colors[1][2]);

                        uint16_t pixmap = 0;
                        for (size_t y_off = 0; y_off < 4; ++y_off) {
                            for (size_t x_off = 0; x_off < 4; ++x_off) {
                                const size_t bit_index = y_off * 4 + x_off; // 0..15
                                const size_t p = (y + y_off) * _width * 3 + (x + x_off) * 3;

                                const auto current_color = std::array{ frame[p], frame[p + 1], frame[p + 2] };

                                if (current_color == colors[0]) {
                                    pixmap |= static_cast<uint16_t>(1) << bit_index;
                                }
                            }
                        }

                        block block;
                        block.mono.colors = static_cast<uint16_t>((idx_color1 << 8) | idx_color0);
                        block.mono.map = pixmap;

                        blocks.emplace_back(preprocessed_block{ block_type::mono, block });
                    } else {
                        block block;
                        for (size_t y_off = 0; y_off < 4; ++y_off) {
                            const size_t p = (y + y_off) * _width * 3 + x * 3;
                            const uint16_t col1 = get_color_index(frame[p], frame[p + 1], frame[p + 2]);
                            const uint16_t col2 = get_color_index(frame[p + 3], frame[p + 4], frame[p + 5]);
                            const uint16_t col3 = get_color_index(frame[p + 6], frame[p + 7], frame[p + 8]);
                            const uint16_t col4 = get_color_index(frame[p + 9], frame[p + 10], frame[p + 11]);
                            block.full.colors[y_off][0] = (col4 << 8) | col3;
                            block.full.colors[y_off][1] = (col2 << 8) | col1;
                        }

                        blocks.emplace_back(preprocessed_block{ block_type::full, block });
                    }
                }
            }

            assert(blocks.size() == _width * _height / 16);

            std::vector<std::vector<preprocessed_block>> rle_blocks;
            std::vector<preprocessed_block> current_chain;
            auto flush = [&]{
                if (current_chain.empty()) return;
                rle_blocks.emplace_back(std::move(current_chain));
                current_chain.clear();
            };

            for (const auto &b : blocks) {
                if (!current_chain.empty() &&
                    (b.type != current_chain.front().type ||
                    (b.type == block_type::solid &&
                    b.data.solid.color != current_chain.front().data.solid.color))) {
                    flush();
                }
                current_chain.push_back(b);
            }
            flush();

            assert(current_chain.size() == 0);
            assert(std::ranges::fold_left(rle_blocks, 0, [](size_t acc, const auto &c) { return acc + c.size(); }) == blocks.size());

            constexpr std::array<size_t, 64> sizetable = {
                1,	2,	3,	4,	5,	6,	7,	8,
                9,	10,	11,	12,	13,	14,	15,	16,
                17,	18,	19,	20,	21,	22,	23,	24,
                25,	26,	27,	28,	29,	30,	31,	32,
                33,	34,	35,	36,	37,	38,	39,	40,
                41,	42,	43,	44,	45,	46,	47,	48,
                49,	50,	51,	52,	53,	54,	55,	56,
                57,	58,	59,	128, 256, 512, 1024, 2048
            };

            const auto get_sizes = [sizetable](const std::vector<preprocessed_block> &c) {
                std::vector<size_t> dp(c.size() + 1, std::numeric_limits<size_t>::max());
                std::vector<size_t> lastSize(c.size() + 1, -1);
                dp[0] = 0;

                for (size_t n = 0; n < sizetable.size(); ++n) {
                    const auto size = sizetable[n];
                    for (size_t m = size; m <= c.size(); ++m) {
                        if (dp[m - size] + 1 < dp[m]) {
                            dp[m] = dp[m - size] + 1;
                            lastSize[m] = n;
                        }
                    }
                }

                if (lastSize[c.size()] == -1) {
                    throw std::runtime_error("Block size could not be encoded");
                }

                std::vector<size_t> sizes;
                auto n = c.size();
                while (n > 0) {
                    sizes.emplace_back(lastSize[n]);
                    n -= sizetable[lastSize[n]];
                }

                return sizes;
            };

            std::vector<chain> chains;
            for (const auto &c : rle_blocks) {
                const auto sizes = get_sizes(c);
                size_t skip = 0;
                for (const auto &size : sizes) {
                    std::vector<block> blocks;
                    if (c.front().type == block_type::full || c.front().type == block_type::mono) {
                        blocks.reserve(sizetable[size]);
                        for (size_t n = skip; n < skip + sizetable[size]; ++n) {
                            blocks.emplace_back(c[n].data);
                        }
                        skip += blocks.size();
                    }
                    chains.emplace_back(chain{
                        .type = c.front().type,
                        .length = size,
                        .data = static_cast<uint8_t>(c.front().type == block_type::solid ? c.front().data.solid.color : 0),
                        .blocks = std::move(blocks),
                    });
                }
            }

            assert(std::ranges::fold_left(chains, 0, [sizetable](size_t acc, const auto &c) { return acc + sizetable[c.length]; }) == blocks.size());
            assert(std::ranges::fold_left(chains, 0, [sizetable](size_t acc, const auto &c) { return acc + (c.blocks.empty() ? sizetable[c.length] : c.blocks.size()); }) == blocks.size());

            frame_chains.emplace_back(std::move(chains));

            std::ranges::copy(frame, last_frame.begin());
        }

        const auto write_chains = [&](const std::vector<chain> &chains, huffman_tree<uint16_t> &type, huffman_tree<uint16_t> &mmap, huffman_tree<uint16_t> &mclr, huffman_tree<uint16_t> &full) {
            for (const auto &chain : chains) {
            const uint16_t type_data = static_cast<uint16_t>(chain.type) | (chain.length << 2) | (chain.data << 8);
            type.write(type_data);

            // std::cout << std::format("Type: {}, Length: {}, Data: {:x}", static_cast<uint16_t>(chain.type), chain.length, chain.data) << std::endl;

            switch (chain.type) {
                case block_type::solid:
                case block_type::void_:
                    break;
                case block_type::full:
                    for (const auto &block : chain.blocks) {
                        for (size_t n = 0; n < 4; ++n) {
                            full.write(block.full.colors[n][0]);
                            full.write(block.full.colors[n][1]);
                        }
                    }
                    break;
                case block_type::mono:
                    for (const auto &block : chain.blocks) {
                        mclr.write(block.mono.colors);
                        mmap.write(block.mono.map);
                    }
                    break;
                default:
                    throw std::runtime_error(std::format("Unsupported chain type: {}", static_cast<uint8_t>(chain.type)));
            }
        }
        };

        for (const auto &chains : frame_chains) {
            write_chains(chains, type, mmap, mclr, full);
        }

        mmap.pack();
        mclr.pack();
        full.pack();
        type.pack();

        bs.flush();
        const auto packed_trees = ss.str();

        write_le<uint32_t>(file, packed_trees.size()); // tree sizes
        write_le<uint32_t>(file, (mmap.size() * 4) + 12);
        write_le<uint32_t>(file, (mclr.size() * 4) + 12);
        write_le<uint32_t>(file, (full.size() * 4) + 12);
        write_le<uint32_t>(file, (type.size() * 4) + 12);

        for (size_t n = 0; n < 7; ++n) {
            write_le<uint32_t>(file, 0); // audio rate
        }

        write_le<uint32_t>(file, 0); // dummy

        std::vector<std::string> frame_data;
        frame_data.reserve(frame_chains.size());
        for (size_t n = 0; n < frame_chains.size(); ++n) {
            const auto &chains = frame_chains[n];
            ss.str("");
            write_chains(chains, type, mmap, mclr, full);
            bs.flush();
            auto data = ss.str();
            const size_t frame_size = (data.size() + (n == 0 ? (256 * 3 + 4) : 0));
            const size_t padding = (4 - (frame_size % 4)) % 4;
            data.append(padding, '\0');
            frame_data.emplace_back(data);
            write_le<uint32_t>(file, frame_size + padding); // last bit indicates keyframe, second last bit is reserved
        }

        for (size_t n = 0; n < _frames.size(); ++n) {
            write_le<uint8_t>(file, n == 0 ? 1 : 0); // frame type (has palette)
        }

        file.write(packed_trees.data(), packed_trees.size());

        for (size_t n = 0; n < frame_data.size(); ++n) {
            if (n == 0) {
                _write_palette(file, palette);
            }
            file.write(frame_data[n].data(), frame_data[n].size());
        }
    }

    void encoder::_write_palette(std::ostream &file, const palette_type &palette) {
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
            static_assert(palmap.size() <= std::numeric_limits<uint_fast8_t>::max());

            for (uint_fast8_t n = 0; n < palmap.size(); ++n) {
                if (palmap[n] >= val) {
                    return n;
                }
            }

            throw std::runtime_error("Color exceeds palmap");
        };

        file.put(static_cast<char>(193)); // length of palette (256 * 3 + 1) / 4

        size_t n = 0;
        for (const auto &color : palette) {
            file.put(get_index(color[0]));
            file.put(get_index(color[1]));
            file.put(get_index(color[2]));
        }

        for (size_t n = 0; n < 3; ++n) {
            file.put(static_cast<char>(0)); // padding
        }
    }
}
