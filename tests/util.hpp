#pragma once

#include <format>
#include <stdexcept>
#include <functional>

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
