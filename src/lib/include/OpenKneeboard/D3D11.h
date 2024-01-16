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

#include <OpenKneeboard/bitflags.h>

#include <shims/winrt/base.h>

#include <memory>

#include <DirectXMath.h>
#include <d3d11_3.h>

namespace OpenKneeboard::D3D11 {

class SavedState final {
 public:
  SavedState(const winrt::com_ptr<ID3D11DeviceContext>&);
  ~SavedState();

  SavedState() = delete;
  SavedState(const SavedState&) = delete;
  SavedState(SavedState&&) = delete;
  SavedState& operator=(const SavedState&) = delete;
  SavedState& operator=(SavedState&&) = delete;

 private:
  struct Impl;
  Impl* mImpl {nullptr};
};

void CopyTextureWithTint(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  DirectX::FXMVECTOR tint);

void CopyTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  float opacity);

void DrawTextureWithTint(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  const RECT& sourceRect,
  const RECT& destRect,
  DirectX::FXMVECTOR tint);

void DrawTextureWithOpacity(
  ID3D11Device* device,
  ID3D11ShaderResourceView* source,
  ID3D11RenderTargetView* dest,
  const RECT& sourceRect,
  const RECT& destRect,
  float opacity);

}// namespace OpenKneeboard::D3D11
