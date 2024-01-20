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

#include <DirectXMath.h>

// Helpers that work with both D3D11 and D3D12
namespace OpenKneeboard::D3D {

/** Helper for converting a 0.0f->1.0f opacity value to a color
 * with premultiplied alpha.
 */
class Opacity final {
 public:
  Opacity() = delete;
  explicit constexpr Opacity(float opacity) noexcept {
    // Assuming premultiplied alpha
    mColor = {opacity, opacity, opacity, opacity};
  }

  constexpr operator DirectX::XMVECTORF32() const noexcept {
    return mColor;
  }

 private:
  DirectX::XMVECTORF32 mColor;
};
}