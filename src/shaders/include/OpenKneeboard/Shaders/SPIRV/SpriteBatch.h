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

namespace OpenKneeboard::Shaders::SPIRV::SpriteBatch::Detail {

#include <OpenKneeboard/Shaders/gen/OpenKneeboard-SpriteBatch-SPIRV-PS.h>
#include <OpenKneeboard/Shaders/gen/OpenKneeboard-SpriteBatch-SPIRV-VS.h>

}// namespace OpenKneeboard::Shaders::SPIRV::SpriteBatch::Detail

namespace OpenKneeboard::Shaders::SPIRV::SpriteBatch {

constexpr std::basic_string_view<unsigned char> PS {
  Detail::g_SpritePixelShader,
  std::size(Detail::g_SpritePixelShader)};

constexpr std::basic_string_view<unsigned char> VS {
  Detail::g_SpriteVertexShader,
  std::size(Detail::g_SpriteVertexShader)};

}// namespace OpenKneeboard::Shaders::SPIRV::SpriteBatch