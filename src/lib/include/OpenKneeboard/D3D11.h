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

#include <OpenKneeboard/D3D.h>
#include <OpenKneeboard/Pixels.h>

#include <shims/winrt/base.h>

#include <directxtk/SpriteBatch.h>

#include <memory>

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <d3d11_3.h>

namespace OpenKneeboard::D3D11 {

class SavedState final {
 public:
  SavedState(const winrt::com_ptr<ID3D11DeviceContext>&);
  SavedState(ID3D11DeviceContext*);
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

using Opacity = ::OpenKneeboard::D3D::Opacity;

/** Wrapper around DirectXTK SpriteBatch which sets the required state on the
 * device first.
 *
 * Handy instead of DirectXTK as all current OpenKneeboard clients either:
 * - pretty much use Direct2D with this being the only D3D, so set the state up
 * just for this
 * - hook into an exisiting render pipeline so can't make assumptions about the
 * device state. These should probably also use the `SavedState` class above.
 *
 * I've also added a helper for `Clear()` which is pretty basic
 * for D3D11, but more involved for other APIs like D3D12; included here for
 * consistency.
 */
class SpriteBatch {
 public:
  SpriteBatch() = delete;
  SpriteBatch(ID3D11Device*);
  ~SpriteBatch();

  void Begin(
    ID3D11RenderTargetView*,
    const PixelSize& rtvSize,
    std::function<void __cdecl()> setCustomShaders = nullptr);
  void Clear(DirectX::XMVECTORF32 color = DirectX::Colors::Transparent);
  void Draw(
    ID3D11ShaderResourceView* source,
    const PixelRect& sourceRect,
    const PixelRect& destRect,
    const DirectX::XMVECTORF32 tint = DirectX::Colors::White);
  void End();

 private:
  winrt::com_ptr<ID3D11Device> mDevice;
  winrt::com_ptr<ID3D11DeviceContext> mDeviceContext;

  std::unique_ptr<DirectX::DX11::SpriteBatch> mDXTKSpriteBatch;

  ID3D11RenderTargetView* mTarget {nullptr};
};

}// namespace OpenKneeboard::D3D11
