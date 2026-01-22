
// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <array>

namespace OpenKneeboard::Vulkan {
class Color : public std::array<float, 4> {};

class Opacity : public Color {
 public:
  Opacity() = delete;
  constexpr Opacity(float opacity)
    : Color {opacity, opacity, opacity, opacity} {}
};

namespace Colors {
constexpr Color White {1, 1, 1, 1};
constexpr Color Transparent {0, 0, 0, 0};
}// namespace Colors

}// namespace OpenKneeboard::Vulkan
