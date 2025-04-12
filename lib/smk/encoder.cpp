#include "encoder.hpp"

#include <algorithm>
#include <stdexcept>
#include <limits>
#include <format>
#include <bit>
#include <sstream>
#include <cassert>

template<typename T>
void write(std::ostream &file, T value) {
    if constexpr (std::endian::native != std::endian::little) {
        value = std::byteswap(value);
    }
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

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

    encoder::encoder(std::ostream &file, uint32_t width, uint32_t height, uint32_t fps) : _file(file), _width(width), _height(height), _fps(fps) {
        if (width % 4 != 0 || height % 4 != 0) {
            throw std::invalid_argument("width and height must be divisible by 4");
        }
    }

    void encoder::encode_frame(const std::span<uint8_t> &frame) {
        palette_type palette;
        size_t pos = 0;
        for (size_t n = 0; n < frame.size(); n += 3) {
            bool found = false;

            for (size_t m = 0; m < pos; ++m) {
                if (palette[m][0] == frame[n] && palette[m][1] == frame[n + 1] && palette[m][2] == frame[n + 2]) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                palette[pos++] = palette_type::value_type{ frame[n], frame[n + 1], frame[n + 2] };

                if (pos >= palette.size()) {
                    throw std::runtime_error("too many colors");
                }
            }
        }

        const auto get_index = [&](const palette_type::value_type &color) {
            for (size_t n = 0; n < palette.size(); ++n) {
                if (palette[n] == color) {
                    return static_cast<uint8_t>(n);
                }
            }

            throw std::runtime_error(std::format("color not found: {} {} {}", color[0], color[1], color[2]));
        };

        struct preprocessed_block {
            block_type type;
            union {
                struct {
                    uint8_t color1;
                    uint8_t color2;
                    uint16_t pattern;
                } mono;
                struct {
                    uint8_t color;
                } solid;
            };
        };

        std::vector<preprocessed_block> blocks;
        for (size_t y = 0; y < _height; y += 4) {
            for (size_t x = 0; x < _width; x += 4) {
                std::array<palette_type::value_type, 3> colors;
                size_t num_colors = 0;
                for (size_t y_off = 0; y_off < 4; ++y_off) {
                    for (size_t x_off = 0; x_off < 4; ++x_off) {
                        const size_t p = (y + y_off) * _width * 3 + (x + x_off) * 3;
                        const auto color = palette_type::value_type{ frame[p], frame[p + 1], frame[p + 2] };

                        if (std::find(colors.begin(), colors.begin() + num_colors, color) == colors.begin() + num_colors) {
                            colors[num_colors++] = color;
                        }

                        if (num_colors >= colors.size()) {
                            break;
                        }
                    }

                    if (num_colors >= colors.size()) {
                        break;
                    }
                }

                if (num_colors < 2) {
                    blocks.emplace_back(preprocessed_block{ block_type::solid, { get_index(colors[0]) } });
                } else if (num_colors == 2) {
                    // uint16_t pattern = 0;
                    // for (size_t x_off = 0; x_off < 4; ++x_off) {
                    //     for (size_t y_off = 0; y_off < 4; ++y_off) {
                    //         const size_t p = (x + x_off) * 3 + (y + y_off) * _width * 3;
                    //         const auto color = get_index(palette_type::value_type{ frame[p], frame[p + 1], frame[p + 2] });
                    //         const uint16_t bit = colors[0] == color ? 0 : 1;
                    //         pattern |= (bit << ());
                    //     }
                    // }
                    // blocks.emplace_back(preprocessed_block{ block_type::mono, { colors[0], colors[1] } });
                    blocks.emplace_back(preprocessed_block{ block_type::solid, { get_index(colors[0]) } });
                } else {
                    // blocks.emplace_back(preprocessed_block{ block_type::full, { colors[0], colors[1], colors[2], colors[3] } });
                    blocks.emplace_back(preprocessed_block{ block_type::solid, { get_index(colors[0]) } });
                }
            }
        }

        assert(blocks.size() == _width * _height / 16);

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

        std::vector<std::pair<size_t, std::vector<preprocessed_block>>> preprocessed_chains;
        std::vector<preprocessed_block> current_chain;
        for (const auto &block : blocks) {
            // if (current_chain.size() < 1 || (block.type == current_chain.back().type && (block.type != block_type::solid || block.solid.color != current_chain.back().solid.color))) {
            //     current_chain.emplace_back(block);
            //     continue;
            // }

            // while (current_chain.size() > 0) {
            //     for (size_t n = sizetable.size(); n > 0; --n) {
            //         if (current_chain.size() >= sizetable[n - 1]) {
            //             preprocessed_chains.emplace_back(n - 1, std::vector<preprocessed_block>(current_chain.begin(), current_chain.begin() + n));
            //             current_chain.erase(current_chain.begin(), current_chain.begin() + n);
            //             break;
            //         }
            //     }
            // }
            preprocessed_chains.emplace_back(0, std::vector<preprocessed_block>{ block });
        }

        std::vector<chain> chains;
        for (const auto &c : preprocessed_chains) {
            chains.emplace_back(chain{
                .type = c.second[0].type,
                .length = c.first,
                .data = c.second[0].solid.color,
                .blocks = {},
            });
        }

        assert(std::ranges::fold_left(chains, 0, [sizetable](size_t acc, const auto &c) { return acc + sizetable[c.length]; }) == blocks.size());

        _frames.emplace_back(frame_data{
            .palette = palette,
            .chains = std::move(chains),
        });

        ++_num_frames;
    }

    encoder::~encoder() {
        _file.write("SMK2", 4);

        write<uint32_t>(_file, _width);
        write<uint32_t>(_file, _height);
        write<uint32_t>(_file, _num_frames);
        write<int32_t>(_file, 1000 / _fps);

        write<uint32_t>(_file, 0); // flags

        for (size_t n = 0; n < 7; ++n) {
            write<uint32_t>(_file, 0); // audio size
        }

        std::ostringstream ss(std::ios::binary);
        bitstream bs(ss);
        huffman_tree<uint16_t> type(bs);
        huffman_tree<uint16_t> mmap(bs);
        huffman_tree<uint16_t> mclr(bs);
        huffman_tree<uint16_t> full(bs);

        for (size_t n = 0; n < _num_frames; ++n) {
            _write_chains(_frames[n].chains, type, mmap, mclr, full);
        }

        mmap.switch_to_write_mode();
        mmap.pack();
        mclr.switch_to_write_mode();
        mclr.pack();
        full.switch_to_write_mode();
        full.pack();
        type.switch_to_write_mode();
        type.pack();

        bs.flush();
        const auto packed_trees = ss.str();

        write<uint32_t>(_file, packed_trees.size()); // tree sizes
        write<uint32_t>(_file, (mmap.size() * 4) + 12);
        write<uint32_t>(_file, (mclr.size() * 4) + 12);
        write<uint32_t>(_file, (full.size() * 4) + 12);
        write<uint32_t>(_file, (type.size() * 4) + 12);

        for (size_t n = 0; n < 7; ++n) {
            write<uint32_t>(_file, 0); // audio rate
        }

        write<uint32_t>(_file, 0); // dummy

        std::vector<std::string> frame_data;
        frame_data.reserve(_num_frames);
        for (const auto &frame : _frames) {
            ss.str("");
            _write_chains(frame.chains, type, mmap, mclr, full);
            bs.flush();
            auto data = ss.str();
            const size_t frame_size = (data.size() + (256 * 3 + 4));
            const size_t padding = (4 - (frame_size % 4)) % 4;
            for (size_t n = 0; n < padding; ++n) {
                data += '\0';
            }
            frame_data.emplace_back(data);
            write<uint32_t>(_file, frame_size + padding); // last bit indicates keyframe
        }

        for (size_t n = 0; n < _num_frames; ++n) {
            write<uint8_t>(_file, 1); // frame type (has palette)
        }

        _file.write(packed_trees.data(), packed_trees.size());

        for (size_t n = 0; n < _num_frames; ++n) {
            _write_palette(_frames[n].palette);
            _file.write(frame_data[n].data(), frame_data[n].size());
        }
    }

    void encoder::_write_chains(const std::vector<chain> &chains, huffman_tree<uint16_t> &type, huffman_tree<uint16_t> &mmap, huffman_tree<uint16_t> &mclr, huffman_tree<uint16_t> &full) {
        for (const auto &chain : chains) {
            switch (chain.type) {
                case block_type::solid: {
                    const uint16_t type_data = static_cast<uint16_t>(chain.type) | (chain.length << 2) | (chain.data << 8);
                    type.write(type_data);
                    break;
                }
                default:
                    throw std::runtime_error("Not implemented");
            }
        }
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

        _file.put(static_cast<char>(193)); // length of palette (256 * 3 + 1) / 4

        size_t n = 0;
        for (const auto &color : palette) {
            _file.put(get_index(color[0]));
            _file.put(get_index(color[1]));
            _file.put(get_index(color[2]));
        }

        for (size_t n = 0; n < 3; ++n) {
            _file.put(static_cast<char>(0)); // padding
        }
    }
}
