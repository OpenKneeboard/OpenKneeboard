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

#include <string>

namespace OpenKneeboard::Shaders::D3D::Viewer::Detail {

#include <OpenKneeboard/Shaders/gen/OpenKneeboard-Viewer-D3D-PS.h>
#include <OpenKneeboard/Shaders/gen/OpenKneeboard-Viewer-D3D-VS.h>

}// namespace OpenKneeboard::Shaders::D3D::Viewer::Detail

namespace OpenKneeboard::Shaders::D3D::Viewer {

constexpr std::basic_string_view<unsigned char> PS {
  Detail::g_ViewerPixelShader,
  std::size(Detail::g_ViewerPixelShader)};

constexpr std::basic_string_view<unsigned char> VS {
  Detail::g_ViewerVertexShader,
  std::size(Detail::g_ViewerVertexShader)};

}// namespace OpenKneeboard::Shaders::D3D::Viewer