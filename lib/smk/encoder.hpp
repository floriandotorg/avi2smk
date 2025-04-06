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

namespace smk {
    class encoder {
    public:
        explicit encoder(std::ostream &file, uint32_t width, uint32_t height, uint32_t fps, uint32_t num_frames);
        void encode_frame(const std::span<uint8_t> &frame);

    private:
        class bitstream {
        public:
            using value_type = uint16_t;

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
            std::map<symbol_type, code_type> _huff_table;

            void _build_huff_table(node *node, code_type code) {
                if (node->symbol.has_value()) {
                    _huff_table[node->symbol.value()] = code;
                    return;
                }

                _build_huff_table(node->zero.get(), code_type{ static_cast<bitstream::value_type>(code.word), code.length + 1 });
                _build_huff_table(node->one.get(), code_type{ static_cast<bitstream::value_type>(code.word | (1 << code.length)), code.length + 1 });
            }
        public:
            huffman_tree(bitstream &bitstream) : _bitstream(bitstream) {}

            void switch_to_write_mode() {
                if (_root != nullptr) {
                    throw std::runtime_error("already in write mode");
                }

                std::vector<std::unique_ptr<node>> queue;
                queue.reserve(_symbol_freq.size());
                std::ranges::transform(_symbol_freq, std::back_inserter(queue), [](const auto &pair) {
                    return std::make_unique<node>(node{ nullptr, nullptr, pair.first, pair.second });
                });

                while (queue.size() > 1) {
                    std::ranges::sort(queue, [](const auto &a, const auto &b) {
                        return a->freq > b->freq;
                    });

                    auto left = std::move(queue.back());
                    queue.pop_back();
                    auto right = std::move(queue.back());
                    queue.pop_back();

                    const auto freq = left->freq + right->freq;
                    queue.push_back(std::make_unique<node>(node{ std::move(left), std::move(right), {}, freq }));
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

                const auto &code = _huff_table[value];
                _bitstream.write(code.word, code.length);
            }

            void _pack(node *node) const {
                if (!node->symbol.has_value()) {
                    _bitstream.write(0b1, 1);
                    _pack(node->zero.get());
                    _pack(node->one.get());
                    return;
                }

                _bitstream.write(0b0, 1);
                _bitstream.write(node->symbol.value(), sizeof(symbol_type) * 8);
            }

            void pack() const {
                if (_root == nullptr) {
                    throw std::runtime_error("not in write mode");
                }

                _bitstream.write(0b1, 1);

                _pack(_root.get());

                _bitstream.write(0b0, 1);
            }
        };

        std::ostream &_file;

        using palette_type = std::array<std::array<uint8_t, 3>, 256>;

        void _write_palette(const palette_type &palette);
    };
}
