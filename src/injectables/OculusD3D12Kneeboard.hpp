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

#include "ID3D12CommandQueueExecuteCommandListsHook.hpp"
#include "OculusKneeboard.hpp"

#include <OpenKneeboard/D3D12/Renderer.hpp>
#include <OpenKneeboard/SHM/D3D12.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/config.hpp>

#include <vector>

#include <d3d11.h>
#include <d3d11on12.h>

#include <directxtk12/GraphicsMemory.h>

namespace OpenKneeboard {

class OculusD3D12Kneeboard final : public OculusKneeboard::Renderer {
 public:
  OculusD3D12Kneeboard();
  virtual ~OculusD3D12Kneeboard();

  void UninstallHook();

  virtual SHM::CachedReader* GetSHM() override;

  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    const PixelSize&) override;

  virtual void RenderLayers(
    ovrTextureSwapChain swapchain,
    uint32_t swapchainTextureIndex,
    const SHM::Snapshot& snapshot,
    const std::span<SHM::LayerSprite>& layers) override;

 private:
  SHM::D3D12::CachedReader mSHM {SHM::ConsumerKind::OculusD3D12};

  ID3D12CommandQueueExecuteCommandListsHook mExecuteCommandListsHook;
  OculusKneeboard mOculusKneeboard;

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;

  using SwapchainBufferResources = D3D12::SwapchainBufferResources;
  using SwapchainResources = D3D12::SwapchainResources;

  std::optional<SwapchainResources> mSwapchain;
  std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
  std::unique_ptr<D3D12::Renderer> mRenderer;

  std::atomic_flag mInitialized;

  void Initialize(ID3D12CommandQueue*);

  void OnID3D12CommandQueue_ExecuteCommandLists(
    ID3D12CommandQueue* this_,
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists,
    const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next);
};
}// namespace OpenKneeboard
