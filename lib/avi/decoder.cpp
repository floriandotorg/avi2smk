#include "decoder.hpp"

#include <bit>
#include <format>
#include <string_view>
#include <stdexcept>

namespace avi {
    template<typename T>
    T read(std::istream &file) {
        T value;
        file.read(reinterpret_cast<char*>(&value), sizeof(T));
        if constexpr (std::endian::native != std::endian::little) {
            value = std::byteswap(value);
        }
        return value;
    }

    void check_signature(std::istream &file, const char* signature) {
        std::array<char, 4> buffer;
        file.read(buffer.data(), buffer.size());
        if (std::string_view(buffer.data(), buffer.size()) != signature) {
            throw std::runtime_error(std::format("Invalid signature: {} (expected {})", buffer, signature));
        }
    }

    void skip_junk(std::istream &file) {
        while (true) {
            std::array<char, 4> buffer;
            file.read(buffer.data(), buffer.size());
            if (std::string_view(buffer.data(), buffer.size()) == "JUNK") {
                file.seekg(read<uint32_t>(file), std::ios::cur);
            } else if (std::string_view(buffer.data(), buffer.size()) == "LIST") {
                break;
            } else {
                throw std::runtime_error(std::format("Invalid signature: {}", buffer));
            }
        }
    }

    decoder::decoder(std::istream &file) : _file(file) {
        check_signature(_file, "RIFF");
        _file.seekg(4, std::ios::cur);
        check_signature(_file, "AVI ");
        check_signature(_file, "LIST");
        _file.seekg(4, std::ios::cur);
        check_signature(_file, "hdrl");
        check_signature(_file, "avih");
        _file.seekg(4, std::ios::cur);
        _fps = 1000000 / read<uint32_t>(_file);
        _file.seekg(12, std::ios::cur);
        _num_frames = read<uint32_t>(_file);
        _file.seekg(12, std::ios::cur);
        _width = read<uint32_t>(_file);
        _height = read<uint32_t>(_file);
        _file.seekg(16, std::ios::cur);
        check_signature(_file, "LIST");
        _file.seekg(4, std::ios::cur);
        check_signature(_file, "strl");
        check_signature(_file, "strh");
        _file.seekg(4, std::ios::cur);
        check_signature(_file, "vids");
        if (read<uint32_t>(_file) != 0) {
            throw std::runtime_error(std::format("Invalid vids type: {}", read<uint32_t>(_file)));
        }
        _file.seekg(48, std::ios::cur);
        check_signature(_file, "strf");
        _file.seekg(18, std::ios::cur);
        const auto bit_per_pixel = read<uint16_t>(_file);
        if (bit_per_pixel != 24) {
            throw std::runtime_error(std::format("Invalid bit per pixel: {}", bit_per_pixel));
        }
        const auto compression_type = read<uint32_t>(_file);
        if (compression_type != 0) {
            throw std::runtime_error(std::format("Invalid compression type: {}", compression_type)  );
        }
        _file.seekg(20, std::ios::cur);
        skip_junk(_file);
        const auto info_size = read<uint32_t>(_file);
        check_signature(_file, "INFO");
        _file.seekg(info_size - 4, std::ios::cur);
        skip_junk(_file);
        _file.seekg(4, std::ios::cur);
        check_signature(_file, "movi");

        if (_width % 4 != 0) {
            throw std::runtime_error(std::format("Width {} is not divisible by 4", _width));
        }

        _frame.resize(_width * _height * 3);
    }

    std::span<uint8_t> decoder::decode_frame() {
        check_signature(_file, "00dc");
        const auto frame_size = read<uint32_t>(_file);
        if (frame_size != _frame.size()) {
            throw std::runtime_error(std::format("Invalid frame size: {} (expected {})", frame_size, _frame.size()));
        }
        _file.read(reinterpret_cast<char*>(_frame.data()), _frame.size());
        return _frame;
    }
}
