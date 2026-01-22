// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <string>

namespace OpenKneeboard::Shaders::Sprite::DXBC::Detail {

#include <OpenKneeboard/Shaders/gen/OpenKneeboard-Sprite-DXBC-PS.hpp>
#include <OpenKneeboard/Shaders/gen/OpenKneeboard-Sprite-DXBC-VS.hpp>

}// namespace OpenKneeboard::Shaders::Sprite::DXBC::Detail

namespace OpenKneeboard::Shaders::Sprite::DXBC {

constexpr std::basic_string_view<unsigned char> PS {
  Detail::g_SpritePixelShader,
  std::size(Detail::g_SpritePixelShader)};

constexpr std::basic_string_view<unsigned char> VS {
  Detail::g_SpriteVertexShader,
  std::size(Detail::g_SpriteVertexShader)};

}// namespace OpenKneeboard::Shaders::Sprite::DXBC
