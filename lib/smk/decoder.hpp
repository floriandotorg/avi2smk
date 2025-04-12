#pragma once

#include <cstdint>
#include <istream>
#include <span>
#include <string>
#include <array>
#include <vector>

namespace smk {
    class decoder {
    public:
        explicit decoder(std::istream &file);
        std::span<uint8_t> decode_frame();

        uint32_t width() const { return _width; }
        uint32_t height() const { return _height; }
        uint32_t num_frames() const { return _num_frames; }
        int32_t framerate() const { return _framerate; }

    private:
        std::istream &_file;
        uint32_t _width;
        uint32_t _height;
        uint32_t _num_frames;
        int32_t _framerate;
        std::vector<uint32_t> _frame_sizes;
        std::vector<uint8_t> _frame_types;

        uint8_t _current_bit;
        uint8_t _current_byte;

        void _init_bitstream();
        bool _bitstream_read_bit();
        uint8_t _bitstream_read_byte();

        struct huff16 {
        std::vector<uint32_t> tree;
        std::array<uint16_t, 3> cache;
        };

        huff16 _mmap;
        huff16 _mclr;
        huff16 _full;
        huff16 _type;

        huff16 _build_hoff16();
        uint16_t _lookup_hoff16(huff16 &tree);
        void _build_hoff16_rec(huff16 &tree, const std::vector<uint16_t> &low_tree, const std::vector<uint16_t> &high_tree, std::string code);

        std::vector<uint16_t> _build_hoff8();
        uint8_t _lookup_hoff8(const std::vector<uint16_t> &tree);
        void _build_hoff8_rec(std::vector<uint16_t> &tree, std::string code);

        using palette = std::array<std::array<uint8_t, 3>, 256>;
        palette _palette;
        void _read_palette();

        size_t _current_frame;
        std::vector<uint8_t> _frame_data;
    };
}
