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

#include <d3d12.h>

#include <directxtk12/GraphicsMemory.h>
#include <directxtk12/SpriteBatch.h>

namespace OpenKneeboard::D3D12 {

using Opacity = ::OpenKneeboard::D3D::Opacity;

class SpriteBatch {
 public:
  SpriteBatch() = delete;
  SpriteBatch(ID3D12Device*, ID3D12CommandQueue*, DXGI_FORMAT destFormat);
  ~SpriteBatch();

  void Begin(
    ID3D12GraphicsCommandList*,
    const D3D12_CPU_DESCRIPTOR_HANDLE& renderTargetView,
    const PixelSize& rtvSize);
  void Clear(DirectX::XMVECTORF32 color = DirectX::Colors::Transparent);

  /**
   * @param sourceSize the entire size of the source resource
   * @param sourceRect the rectangle of the source resource to copy; if we're
   * not drawing the entire source resource, the size of this rect will be
   * smaller than `sourceSize`.
   */
  void Draw(
    const D3D12_GPU_DESCRIPTOR_HANDLE& sourceShaderResourceView,
    const PixelSize& sourceSize,
    const PixelRect& sourceRect,
    const PixelRect& destRect,
    const DirectX::XMVECTORF32 tint = DirectX::Colors::White);
  void End();

 private:
  winrt::com_ptr<ID3D12Device> mDevice;

  std::unique_ptr<DirectX::DX12::SpriteBatch> mDXTKSpriteBatch;
  // DXTK12 SpriteBatch depends on an instance of this existing
  std::unique_ptr<DirectX::DX12::GraphicsMemory> mDXTKGraphicsMemory;

  ID3D12GraphicsCommandList* mCommandList {nullptr};
  D3D12_CPU_DESCRIPTOR_HANDLE mRenderTargetView {};
  PixelSize mRenderTargetViewSize {};
};
}// namespace OpenKneeboard::D3D12