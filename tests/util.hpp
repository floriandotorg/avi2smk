#pragma once

#include <format>
#include <stdexcept>
#include <functional>

// we need to include these to prevent problems with redefining private
#include <cstdint>
#include <ostream>
#include <istream>
#include <span>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <cstddef>
#include <cassert>
#include <iterator>
#include <ranges>
#include <cstdint>
#include <span>
#include <array>
#include <memory>
#include <optional>
#include <limits>
#include <climits>

#define private public
#include "../lib/smk/encoder.hpp"
#undef private

#define private public: decoder(std::istream &file, bool testing_only) : _file(file) {}; public
#include "../lib/smk/decoder.hpp"
#undef private

template <typename T, typename U>
void expect_eq(const T &actual, const U &expected) {
    if (actual != expected) {
        throw std::runtime_error(std::format("Expected {} but got {}", expected, actual));
    }
}

void expect_throw(const std::function<void()> &func) {
    bool threw = false;
    try {
        func();
    } catch (const std::exception &e) {
        threw = true;
    }

    if (!threw) {
        throw std::runtime_error("Expected exception but got none");
    }
}
