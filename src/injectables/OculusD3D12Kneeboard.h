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

// clang-format off
#include <Unknwn.h>
#include <winrt/base.h>
// clang-format on

#include <d3d11on12.h>
#include <d3d11.h>

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
    const SHM::Config& config) override;

  virtual bool Render(
    ovrSession session,
    ovrTextureSwapChain swapChain,
    const SHM::Snapshot& snapshot) override;

 private:
  ID3D12CommandQueueExecuteCommandListsHook mExecuteCommandListsHook;
  OculusKneeboard mOculusKneeboard;

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D11Device> m11on12;
  winrt::com_ptr<ID3D11DeviceContext> m11on12Context;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;
  winrt::com_ptr<ID3D12DescriptorHeap> mDescriptorHeap;
  winrt::com_ptr<ID3D12CommandAllocator> mCommandAllocator;

  void OnID3D12CommandQueue_ExecuteCommandLists(
    ID3D12CommandQueue* this_,
    UINT NumCommandLists,
    ID3D12CommandList* const* ppCommandLists,
    const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next);
};
}// namespace OpenKneeboard
