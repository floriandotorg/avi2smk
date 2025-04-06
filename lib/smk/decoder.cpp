#include "decoder.hpp"

#include <bit>
#include <cstring>
#include <algorithm>
#include <format>
#include <string_view>
#include <stdexcept>

namespace smk {
    constexpr static uint32_t HUFF8_BRANCH = 0x8000;
    constexpr static uint32_t HUFF8_LEAF_MASK = 0x7FFF;

    constexpr static uint32_t HUFF16_BRANCH = 0x80000000;
    constexpr static uint32_t HUFF16_LEAF_MASK = 0x3FFFFFFF;
    constexpr static uint32_t HUFF16_CACHE = 0x40000000;

    template<typename T>
    T read(std::istream &file) {
        T value;
        file.read(reinterpret_cast<char*>(&value), sizeof(T));
        if constexpr (std::endian::native != std::endian::little) {
            value = std::byteswap(value);
        }
        return value;
    }

    enum class frame_type : uint8_t {
        mono = 0,
        full = 1,
        void_ = 2,
        solid = 3,
    };

    decoder::decoder(std::istream &file) : _file(file) {
        std::array<char, 4> signature;
        file.read(signature.data(), signature.size());
        if (std::string_view(signature.data(), signature.size()) != "SMK2") {
            throw std::runtime_error(std::format("Invalid SMK signature: {}", signature));
        }

        _width = read<uint32_t>(_file);
        _height = read<uint32_t>(_file);
        _num_frames = read<uint32_t>(_file);
        _framerate = read<int32_t>(_file);
        if (_framerate > 0) {
            _framerate = 1000 / _framerate;
        } else if (_framerate < 0) {
            _framerate = 100000 / -_framerate;
        } else {
            _framerate = 10;
        }

        auto flags = read<uint32_t>(_file);
        if (flags != 0) {
            throw std::runtime_error(std::format("Unsupported flags: {}", flags));
        }

        _file.seekg(28, std::ios::cur);
        auto trees_size = read<uint32_t>(_file);
        _file.seekg(48, std::ios::cur);

        _frame_sizes.resize(_num_frames);
        _file.read(reinterpret_cast<char*>(_frame_sizes.data()), _frame_sizes.size() * sizeof(decltype(_frame_sizes)::value_type));

        _frame_types.resize(_num_frames);
        _file.read(reinterpret_cast<char*>(_frame_types.data()), _frame_types.size());

        for (auto type : _frame_types) {
            if ((type & ~0x01) != 0) {
                throw std::runtime_error("Audio is not supported");
            }
        }

        const auto end_of_trees = _file.tellg() + static_cast<std::istream::pos_type>(trees_size);

        _init_bitstream();
        _mmap = _build_hoff16();
        _mclr = _build_hoff16();
        _full = _build_hoff16();
        _type = _build_hoff16();

        _file.seekg(end_of_trees);

        if (_width % 4 != 0 || _height % 4 != 0) {
            throw std::runtime_error("Width and height must be divisible by 4");
        }

        std::ranges::fill(_palette, palette::value_type{0x00, 0x00, 0x00});
        _current_frame = 0;
        _frame_data.resize(_width * _height * 3);
        std::ranges::fill(_frame_data, 0);
    }

    std::span<uint8_t> decoder::decode_frame() {
        const auto end_of_frame = _file.tellg() + static_cast<std::istream::pos_type>(_frame_sizes[_current_frame]);

        if (_frame_types[_current_frame] & 0x01) {
            _read_palette();
        }

        constexpr size_t sizetable[64] = {
            1,	2,	3,	4,	5,	6,	7,	8,
            9,	10,	11,	12,	13,	14,	15,	16,
            17,	18,	19,	20,	21,	22,	23,	24,
            25,	26,	27,	28,	29,	30,	31,	32,
            33,	34,	35,	36,	37,	38,	39,	40,
            41,	42,	43,	44,	45,	46,	47,	48,
            49,	50,	51,	52,	53,	54,	55,	56,
            57,	58,	59,	128, 256, 512, 1024, 2048
        };

        std::ranges::fill(_mmap.cache, 0);
        std::ranges::fill(_mclr.cache, 0);
        std::ranges::fill(_full.cache, 0);
        std::ranges::fill(_type.cache, 0);

        _init_bitstream();

        uint8_t *t = _frame_data.data();
        size_t row = 0, col = 0;
        while (row < _height) {
            const auto block = _lookup_hoff16(_type);

            const auto type = block & 0x0003;
            const auto blocklen = (block & 0x00FC) >> 2;
            const auto typedata = (block & 0xFF00) >> 8;

            for (size_t n = 0; n < sizetable[blocklen] && row < _height; ++n) {
                auto skip = (row * _width * 3) + col * 3;

                switch (static_cast<frame_type>(type)) {
                    case frame_type::mono: {
                        const auto colors = _lookup_hoff16(_mclr);
                        const auto map = _lookup_hoff16(_mmap);

                        const auto color1 = _palette[colors & 0xFF00 >> 8];
                        const auto color2 = _palette[colors & 0xFF];

                        for (size_t n = 0; n < 4; ++n) {
                            for (size_t m = 0; m < 4; ++m) {
                                if (map & (1 << (n * 4 + m))) {
                                    std::copy(color1.begin(), color1.end(), t + skip + m * 3);
                                } else {
                                    std::copy(color2.begin(), color2.end(), t + skip + m * 3);
                                }
                            }

                            skip += _width * 3;
                        }

                        break;
                    }

                    case frame_type::full: {
                        for (size_t n = 0; n < 4; ++n) {
                            auto full = _lookup_hoff16(_full);

                            const auto color1 = _palette[full & 0xFF00 >> 8];
                            const auto color2 = _palette[full & 0xFF];

                            std::copy(color1.begin(), color1.end(), t + skip + 3 * 3);
                            std::copy(color2.begin(), color2.end(), t + skip + 2 * 3);

                            full = _lookup_hoff16(_full);

                            const auto color3 = _palette[full & 0xFF00 >> 8];
                            const auto color4 = _palette[full & 0xFF];

                            std::copy(color3.begin(), color3.end(), t + skip + 3);
                            std::copy(color4.begin(), color4.end(), t + skip);

                            skip += _width * 3;
                        }

                        break;
                    }

                    case frame_type::void_:
                        break;

                    case frame_type::solid: {
                        const auto color = _palette[typedata & 0xFF];
                        for (size_t n = 0; n < 4; ++n) {
                            for (size_t m = 0; m < 4; ++m) {
                                std::copy(color.begin(), color.end(), t + skip + m * 3);
                            }

                            skip += _width * 3;
                        }

                        break;
                    }

                    default:
                        throw std::runtime_error(std::format("Invalid block type: {}", type));
                }

                col += 4;
                if (col >= _width) {
                    col = 0;
                    row += 4;
                }
            }
        }

        ++_current_frame;
        _file.seekg(end_of_frame);

        return _frame_data;
    }

    void decoder::_init_bitstream() {
        _current_byte = _file.get();
        _current_bit = 0;
    }

    bool decoder::_bitstream_read_bit() {
        const bool result = (_current_byte >> _current_bit) & 1;

        if (++_current_bit > 7) {
            _current_byte = _file.get();
            _current_bit = 0;
        }

        return result;
    }

    uint8_t decoder::_bitstream_read_byte() {
        if (_current_bit == 0) {
            const uint8_t result = _current_byte;
            _current_byte = _file.get();
            return result;
        }

        const uint8_t result = _current_byte >> _current_bit;
        _current_byte = _file.get();

        return result | (_current_byte << (8 - _current_bit) & 0xFF);
    }

    decoder::huff16 decoder::_build_hoff16() {
        if (!_bitstream_read_bit()) {
            throw std::runtime_error("Huff16 not present");
        }

        const auto low_tree = _build_hoff8();
        const auto high_tree = _build_hoff8();

        huff16 tree;

        tree.cache[0] = _bitstream_read_byte() | (_bitstream_read_byte() << 8);
        tree.cache[1] = _bitstream_read_byte() | (_bitstream_read_byte() << 8);
        tree.cache[2] = _bitstream_read_byte() | (_bitstream_read_byte() << 8);

        _build_hoff16_rec(tree, low_tree, high_tree);

        if (_bitstream_read_bit()) {
            throw std::runtime_error("Error reading huff16");
        }

        return tree;
    }

    uint16_t decoder::_lookup_hoff16(huff16 &tree) {
        size_t index = 0;
        while (tree.tree[index] & HUFF16_BRANCH) {
            if (_bitstream_read_bit()) {
                index = tree.tree[index] & HUFF16_LEAF_MASK;
            } else {
                ++index;
            }
        }

        auto value = tree.tree[index];
        if (value & HUFF16_CACHE) {
            value = tree.cache[value & HUFF16_LEAF_MASK];
        }

        if (value != tree.cache[0]) {
            std::rotate(tree.cache.begin(), tree.cache.begin() + 2, tree.cache.begin() + 3);
            tree.cache[0] = value;
        }

        return value;
    }

    void decoder::_build_hoff16_rec(huff16 &tree, const std::vector<uint16_t> &low_tree, const std::vector<uint16_t> &high_tree) {
        if (_bitstream_read_bit()) {
            const auto branch = tree.tree.size();
            tree.tree.push_back(0);
            _build_hoff16_rec(tree, low_tree, high_tree);
            tree.tree[branch] = HUFF16_BRANCH | tree.tree.size();
            _build_hoff16_rec(tree, low_tree, high_tree);
        } else {
            uint32_t value = _lookup_hoff8(low_tree) | (_lookup_hoff8(high_tree) << 8);

            if (value == tree.cache[0]) {
                value = HUFF16_CACHE;
            } else if (value == tree.cache[1]) {
                value = HUFF16_CACHE | 1;
            } else if (value == tree.cache[2]) {
                value = HUFF16_CACHE | 2;
            }

            tree.tree.push_back(value);
        }
    }

    std::vector<uint16_t> decoder::_build_hoff8() {
        if (!_bitstream_read_bit()) {
            throw std::runtime_error("Huff8 not present");
        }

        std::vector<uint16_t> tree;
        tree.reserve(511);
        _build_hoff8_rec(tree);

        if (_bitstream_read_bit()) {
            throw std::runtime_error("Error reading huff8");
        }

        return tree;
    }

    void decoder::_build_hoff8_rec(std::vector<uint16_t> &tree) {
        if (_bitstream_read_bit()) {
            const auto branch = tree.size();
            tree.push_back(0);
            _build_hoff8_rec(tree);
            tree[branch] = HUFF8_BRANCH | tree.size();
            _build_hoff8_rec(tree);
        } else {
            const char value = _bitstream_read_byte();
            tree.push_back(value);
        }
    }

    uint8_t decoder::_lookup_hoff8(const std::vector<uint16_t> &tree) {
        size_t index = 0;
        while (tree[index] & HUFF8_BRANCH) {
            if (_bitstream_read_bit()) {
                index = tree[index] & HUFF8_LEAF_MASK;
            } else {
                ++index;
            }
        }
        return tree[index];
    }

    void decoder::_read_palette() {
        palette old_palette;
        std::ranges::copy(_palette, old_palette.begin());

        constexpr uint8_t palmap[64] = {
            0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
            0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
            0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D,
            0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
            0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E,
            0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
            0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF,
            0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
        };

        palette::iterator n = _palette.begin();
        const auto palette_end = _file.tellg() + static_cast<std::istream::pos_type>(_file.get() * 4);
        while (_file.tellg() < palette_end) {
            const uint8_t block = _file.get();
            if (block & 0x80) {
                n += (block & 0x7F) + 1;
            } else if (block & 0x40) {
                const uint8_t c = (block & 0x3F) + 1;
                const uint8_t s = _file.get();
                n = std::ranges::copy_n(old_palette.begin() + s, c, n).out;
            } else {
                const uint8_t r = palmap[block & 0x3F];
                const uint8_t g = palmap[_file.get() & 0x3F];
                const uint8_t b = palmap[_file.get() & 0x3F];
                *n++ = {r, g, b};
            }
        }
        _file.seekg(palette_end);
    }
}
