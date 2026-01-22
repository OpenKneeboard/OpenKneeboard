// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Shaders/SpriteBatch.hpp>

#include <d3d12.h>

#include <string>

namespace OpenKneeboard::Shaders::SpriteBatch::DXIL::Detail {

#include <OpenKneeboard/Shaders/gen/OpenKneeboard-SpriteBatch-DXIL-PS.hpp>
#include <OpenKneeboard/Shaders/gen/OpenKneeboard-SpriteBatch-DXIL-VS.hpp>

}// namespace OpenKneeboard::Shaders::SpriteBatch::DXIL::Detail

namespace OpenKneeboard::Shaders::SpriteBatch::DXIL {

constexpr D3D12_SHADER_BYTECODE PS {
  Detail::g_SpritePixelShader,
  std::size(Detail::g_SpritePixelShader)};

constexpr D3D12_SHADER_BYTECODE VS {
  Detail::g_SpriteVertexShader,
  std::size(Detail::g_SpriteVertexShader)};

}// namespace OpenKneeboard::Shaders::SpriteBatch::DXIL
