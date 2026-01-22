// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <DirectXMath.h>
#include <dxgi.h>

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

  constexpr operator DirectX::XMVECTORF32() const noexcept { return mColor; }

 private:
  DirectX::XMVECTORF32 mColor;
};

}// namespace OpenKneeboard::D3D
