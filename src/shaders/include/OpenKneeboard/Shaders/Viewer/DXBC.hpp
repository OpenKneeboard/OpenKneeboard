// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <string>

namespace OpenKneeboard::Shaders::Viewer::DXBC::Detail {

#include <OpenKneeboard/Shaders/gen/OpenKneeboard-Viewer-DXBC-PS.hpp>
#include <OpenKneeboard/Shaders/gen/OpenKneeboard-Viewer-DXBC-VS.hpp>

}// namespace OpenKneeboard::Shaders::Viewer::DXBC::Detail

namespace OpenKneeboard::Shaders::Viewer::DXBC {

constexpr std::basic_string_view<unsigned char> PS {
  Detail::g_ViewerPixelShader,
  std::size(Detail::g_ViewerPixelShader)};

constexpr std::basic_string_view<unsigned char> VS {
  Detail::g_ViewerVertexShader,
  std::size(Detail::g_ViewerVertexShader)};

}// namespace OpenKneeboard::Shaders::Viewer::DXBC
