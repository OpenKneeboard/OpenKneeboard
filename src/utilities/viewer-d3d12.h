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

#include "viewer.h"

#include <OpenKneeboard/D3D12.h>
#include <OpenKneeboard/SHM/D3D12.h>

#include <shims/winrt/base.h>

#include <directxtk12/DescriptorHeap.h>

namespace OpenKneeboard::Viewer {

class D3D12Renderer final : public Renderer {
 public:
  D3D12Renderer() = delete;
  D3D12Renderer(IDXGIAdapter*);
  virtual ~D3D12Renderer();
  virtual SHM::CachedReader* GetSHM() override;

  std::wstring_view GetName() const noexcept override;

  virtual void Initialize(uint8_t swapchainLength) override;

  virtual void SaveTextureToFile(
    SHM::IPCClientTexture*,
    const std::filesystem::path&) override;

  virtual uint64_t Render(
    SHM::IPCClientTexture* sourceTexture,
    const PixelRect& sourceRect,
    HANDLE destTexture,
    const PixelSize& destTextureDimensions,
    const PixelRect& destRect,
    HANDLE fence,
    uint64_t fenceValueIn) override;

 private:
  SHM::D3D12::CachedReader mSHM {SHM::ConsumerKind::Viewer};

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;
  winrt::com_ptr<ID3D12CommandAllocator> mCommandAllocator;
  winrt::com_ptr<ID3D12GraphicsCommandList> mCommandList;

  std::unique_ptr<D3D12::SpriteBatch> mSpriteBatch;

  HANDLE mDestHandle {};
  winrt::com_ptr<ID3D12Resource> mDestTexture;
  std::unique_ptr<DirectX::DescriptorHeap> mDestRTVHeap;

  HANDLE mFenceHandle {};
  winrt::com_ptr<ID3D12Fence> mFence;

  uint64_t mFenceValue {};

  void SaveTextureToFile(ID3D12Resource*, const std::filesystem::path&);
};

}// namespace OpenKneeboard::Viewer