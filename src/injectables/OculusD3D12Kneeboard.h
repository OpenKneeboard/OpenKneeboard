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

#include <OpenKneeboard/D3D11On12.h>
#include <OpenKneeboard/config.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <shims/winrt/base.h>

#include <vector>

#include "ID3D12CommandQueueExecuteCommandListsHook.h"
#include "OculusKneeboard.h"

namespace OpenKneeboard {

class OculusD3D12Kneeboard final : public OculusKneeboard::Renderer {
 public:
  OculusD3D12Kneeboard();
  virtual ~OculusD3D12Kneeboard();

  void UninstallHook();

  virtual ovrTextureSwapChain CreateSwapChain(
    ovrSession session,
    uint8_t layerIndex) override;

  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot,
    uint8_t layerIndex,
    const VRKneeboard::RenderParameters&) override final;

  virtual SHM::ConsumerKind GetConsumerKind() const override {
    return SHM::ConsumerKind::OculusD3D12;
  }

  virtual winrt::com_ptr<ID3D11Device> GetD3D11Device() const {
    return mDeviceResources.mDevice11;
  }

 private:
  ID3D12CommandQueueExecuteCommandListsHook mExecuteCommandListsHook;
  OculusKneeboard mOculusKneeboard;
  std::array<
    std::vector<std::shared_ptr<D3D11::IRenderTargetViewFactory>>,
    MaxLayers>
    mRenderTargetViews;

  D3D11On12::DeviceResources mDeviceResources;

  void OnID3D12CommandQueue_ExecuteCommandLists(
    ID3D12CommandQueue* this_,
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists,
    const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next);
};
}// namespace OpenKneeboard
