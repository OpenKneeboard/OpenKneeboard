
/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <array>

namespace OpenKneeboard::Vulkan {
class Color : public std::array<float, 4> {};

class Opacity : public Color {
 public:
  Opacity() = delete;
  constexpr Opacity(float opacity)
    : Color {opacity, opacity, opacity, opacity} {
  }
};

namespace Colors {
constexpr Color White {1, 1, 1, 1};
constexpr Color Transparent {0, 0, 0, 0};
}// namespace Colors

}// namespace OpenKneeboard::Vulkan