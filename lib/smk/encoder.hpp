#pragma once

#include <cstdint>
#include <ostream>
#include <span>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <cstddef>
#include <cassert>
#include <iterator>
#include <ranges>
#include <memory>
#include <optional>
#include <array>
#include <climits>

namespace smk {
    class encoder {
    public:
        explicit encoder(std::ostream &file, uint32_t width, uint32_t height, uint32_t fps);
        ~encoder();

        void encode_frame(const std::span<uint8_t> &frame);

    private:
        class bitstream {
        public:
            using value_type = uint32_t;

            bitstream(std::ostream &file);
            void write(value_type value, uint8_t length);
            void flush();

        private:
            std::ostream &_file;
            uint8_t _buf = 0;
            uint8_t _bits_in_buf = 0;
        };

        template <typename T>
        class huffman_tree {
        public:
            using symbol_type = T;

        private:
            constexpr static bool is_huff16 = std::numeric_limits<symbol_type>::digits >= 16;

            bitstream &_bitstream;

            std::map<symbol_type, size_t> _symbol_freq;

            struct node {
                std::unique_ptr<node> zero;
                std::unique_ptr<node> one;
                std::optional<symbol_type> symbol;
                size_t freq;
            };
            std::unique_ptr<node> _root = nullptr;

            struct code_type {
                bitstream::value_type word;
                size_t length;
            };


            void _build_huff_table(node *node, code_type code) {
                if (node->symbol.has_value()) {
                    _huff_table[node->symbol.value()] = code;
                    return;
                }

                _build_huff_table(node->zero.get(), code_type{ static_cast<bitstream::value_type>(code.word), code.length + 1 });
                _build_huff_table(node->one.get(), code_type{ static_cast<bitstream::value_type>(code.word | (1 << code.length)), code.length + 1 });
            }

            std::array<symbol_type, 3> _escape_values;
        public:
            std::map<symbol_type, code_type> _huff_table;
            huffman_tree(bitstream &bitstream) : _bitstream(bitstream) {}

            void switch_to_write_mode() {
                if (_root != nullptr) {
                    throw std::runtime_error("already in write mode");
                }

                if constexpr (is_huff16) {
                    size_t n = 0;
                    for (uint16_t symbol = 1; symbol < std::numeric_limits<symbol_type>::max() && n < _escape_values.size(); ++symbol) {
                        if (!_symbol_freq.contains(symbol)) {
                            _escape_values[n++] = symbol;
                        }
                    }

                    if (n != _escape_values.size()) {
                        throw std::runtime_error("could not find enough escape values");
                    }
                }

                std::vector<std::unique_ptr<node>> queue;
                queue.reserve(_symbol_freq.size());
                std::ranges::transform(_symbol_freq, std::back_inserter(queue), [](const auto &pair) {
                    return std::make_unique<node>(node{ nullptr, nullptr, pair.first, pair.second });
                });

                if constexpr (is_huff16) {
                    for (size_t n = 0; n < _escape_values.size(); ++n) {
                        queue.emplace_back(std::make_unique<node>(node{ nullptr, nullptr, _escape_values[n], std::numeric_limits<size_t>::max() }));
                    }
                }

                while (queue.size() > 1) {
                    std::ranges::sort(queue, [](const auto &a, const auto &b) {
                        return a->freq > b->freq;
                    });

                    auto left = std::move(queue.back());
                    queue.pop_back();
                    auto right = std::move(queue.back());
                    queue.pop_back();

                    const auto freq = left->freq + right->freq;
                    queue.emplace_back(std::make_unique<node>(node{
                        .zero = std::move(left),
                        .one = std::move(right),
                        .symbol = {},
                        .freq = freq,
                    }));
                }

                assert(queue.size() == 1);

                _root = std::move(queue.back());

                _build_huff_table(_root.get(), {});
            }

            void write(symbol_type value) {
                if (_root == nullptr) {
                    ++_symbol_freq[value];
                    return;
                }

                const auto it = _huff_table.find(value);
                if (it == _huff_table.end()) {
                    throw std::runtime_error("symbol not found in huffman table");
                }

                const auto &code = it->second;
                _bitstream.write(code.word, code.length);
            }

            void _pack(node *node, huffman_tree<uint8_t> *high_byte_tree, huffman_tree<uint8_t> *low_byte_tree) const {
                if (!node->symbol.has_value()) {
                    _bitstream.write(0b1, 1);
                    _pack(node->zero.get(), high_byte_tree, low_byte_tree);
                    _pack(node->one.get(), high_byte_tree, low_byte_tree);
                    return;
                }

                _bitstream.write(0b0, 1);

                if constexpr (is_huff16) {
                    low_byte_tree->write(node->symbol.value() & 0xFF);
                    high_byte_tree->write(node->symbol.value() >> 8);
                } else {
                    _bitstream.write(node->symbol.value(), sizeof(symbol_type) * CHAR_BIT);
                }
            }

            void pack() const {
                if (_root == nullptr) {
                    throw std::runtime_error("not in write mode");
                }

                _bitstream.write(0b1, 1);

                if constexpr (is_huff16) {
                    huffman_tree<uint8_t> low_byte_tree(_bitstream);
                    huffman_tree<uint8_t> high_byte_tree(_bitstream);

                    for (const auto &symbol : _huff_table) {
                        low_byte_tree.write(symbol.first & 0xFF);
                        high_byte_tree.write(symbol.first >> 8);
                    }

                    low_byte_tree.switch_to_write_mode();
                    low_byte_tree.pack();

                    high_byte_tree.switch_to_write_mode();
                    high_byte_tree.pack();

                    for (size_t n = 0; n < _escape_values.size(); ++n) {
                        _bitstream.write(_escape_values[n], sizeof(typename decltype(_escape_values)::value_type) * CHAR_BIT);
                    }

                    _pack(_root.get(), &high_byte_tree, &low_byte_tree);
                } else {
                    _pack(_root.get(), nullptr, nullptr);
                }

                _bitstream.write(0b0, 1);
            }

            size_t _size(node *node) const {
                if (node->symbol.has_value()) {
                    return 1;
                }

                return 1 + _size(node->zero.get()) + _size(node->one.get());
            }

            size_t size() const {
                return _size(_root.get());
            }
        };

        std::ostream &_file;

        using palette_type = std::array<std::array<uint8_t, 3>, 256>;

        void _write_palette(const palette_type &palette);

        size_t _num_frames = 0;

        enum class block_type : uint8_t {
            mono = 0,
            full = 1,
            void_ = 2,
            solid = 3,
        };

        union block {
            struct {
                uint8_t color1;
                uint8_t color2;
            } mono;
            struct {
                std::array<uint8_t, 16> colors;
            } full;
        };

        struct chain {
            block_type type;
            size_t length;
            uint8_t data;
            std::vector<block> blocks;
        };

        struct frame_data {
            palette_type palette;
            std::vector<chain> chains;
        };

        std::vector<frame_data> _frames;

        uint32_t _width;
        uint32_t _height;
        uint32_t _fps;

        void _write_chains(const std::vector<chain> &chains, huffman_tree<uint16_t> &type, huffman_tree<uint16_t> &mmap, huffman_tree<uint16_t> &mclr, huffman_tree<uint16_t> &full);
    };
}
