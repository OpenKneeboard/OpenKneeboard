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
#include <OpenKneeboard/D3D11On12.h>
#include <OpenKneeboard/SHM.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

#include <shims/winrt/base.h>

namespace OpenKneeboard::D3D11On12 {

thread_local uint64_t DeviceResources::mFenceValue {0};

RenderTargetView::RenderTargetView(
  const DeviceResources& deviceResources,
  const winrt::com_ptr<ID3D12Resource>& texture12,
  const winrt::com_ptr<ID3D11Texture2D>& texture11,
  const winrt::com_ptr<ID3D11Texture2D>& bufferTexture11,
  const winrt::com_ptr<ID3D11RenderTargetView>& renderTargetView)
  : mDeviceResources(deviceResources),
    mTexture12(texture12),
    mTexture11(texture11),
    mBufferTexture11(bufferTexture11),
    mRenderTargetView(renderTargetView) {
  if (!mBufferTexture11) {
    auto resources = static_cast<ID3D11Resource*>(texture11.get());
    deviceResources.m11on12->AcquireWrappedResources(&resources, 1);
  }
}

RenderTargetView::~RenderTargetView() noexcept {
  if (!mBufferTexture11) {
    auto resources = static_cast<ID3D11Resource*>(mTexture11.get());
    mDeviceResources.m11on12->ReleaseWrappedResources(&resources, 1);
    return;
  }

  // If we get here, the varjo double-buffer quirk is enabled
  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::check_hresult(mDeviceResources.mDevice12->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    mDeviceResources.mAllocator12.get(),
    nullptr,
    IID_PPV_ARGS(commandList.put())));
  winrt::com_ptr<ID3D12Resource> bufferTexture12;
  winrt::check_hresult(mDeviceResources.m11on12->UnwrapUnderlyingResource(
    mBufferTexture11.get(),
    mDeviceResources.mCommandQueue12.get(),
    IID_PPV_ARGS(bufferTexture12.put())));
  // We just need to queue this up, not wait for it, as we wait for
  // sync on the command queue once we're done
  mDeviceResources.mContext11->Flush();

  D3D12_RESOURCE_BARRIER inBarrier {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = {
        mTexture12.get(),
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_DEST,
      },
    };
  commandList->ResourceBarrier(1, &inBarrier);
  commandList->CopyResource(mTexture12.get(), bufferTexture12.get());

  D3D12_RESOURCE_BARRIER outBarrier {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Transition = {
        mTexture12.get(),
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
      },
    };

  commandList->ResourceBarrier(1, &outBarrier);
  winrt::check_hresult(commandList->Close());

  auto queue = mDeviceResources.mCommandQueue12.get();

  const auto FENCE_VALUE_BEFORE_COMMAND_LIST = ++mDeviceResources.mFenceValue;
  const auto FENCE_VALUE_AFTER_COMMAND_LIST = ++mDeviceResources.mFenceValue;

  const auto commandLists = static_cast<ID3D12CommandList*>(commandList.get());
  queue->ExecuteCommandLists(1, &commandLists);

  auto fence = mDeviceResources.mFence12.get();
  auto fenceValue = ++mDeviceResources.mFenceValue;
  winrt::handle fenceEvent {
    CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS)};
  fence->SetEventOnCompletion(fenceValue, fenceEvent.get());
  winrt::check_hresult(queue->Signal(fence, fenceValue));
  winrt::check_hresult(mDeviceResources.m11on12->ReturnUnderlyingResource(
    mBufferTexture11.get(), 1, &fenceValue, &fence));
  traceprint("Waiting for D3D11on12 fence...");
  WaitForSingleObject(fenceEvent.get(), INFINITE);
  traceprint("Done.");
}

ID3D11RenderTargetView* RenderTargetView::Get() const {
  return mRenderTargetView.get();
}

RenderTargetViewFactory::RenderTargetViewFactory(
  const DeviceResources& deviceResources,
  const winrt::com_ptr<ID3D12Resource>& texture12,
  DXGI_FORMAT format,
  Flags flags)
  : mDeviceResources(deviceResources), mTexture12(texture12) {
  D3D11_RESOURCE_FLAGS resourceFlags11 {};
  if (!static_cast<bool>(flags & Flags::DoubleBuffer)) {
    resourceFlags11.BindFlags = D3D11_BIND_RENDER_TARGET;
  }
  winrt::check_hresult(deviceResources.m11on12->CreateWrappedResource(
    texture12.get(),
    &resourceFlags11,
    D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_STATE_COMMON,
    IID_PPV_ARGS(mTexture11.put())));

  if (static_cast<bool>(flags & Flags::DoubleBuffer)) {
    const auto t12Desc = texture12->GetDesc();
    // Not using SHM::CreateCompatibleTexture as we need to match the size
    D3D11_TEXTURE2D_DESC bufferDesc {
      .Width = static_cast<UINT>(t12Desc.Width),
      .Height = static_cast<UINT>(t12Desc.Height),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = format,
      .SampleDesc = {1, 0},
      .BindFlags = SHM::DEFAULT_D3D11_BIND_FLAGS,
      .MiscFlags = SHM::DEFAULT_D3D11_MISC_FLAGS,
    };
    winrt::check_hresult(deviceResources.mDevice11->CreateTexture2D(
      &bufferDesc, nullptr, mBufferTexture11.put()));
    const std::string_view name {"OKB_D3D1211on12-DoubleBuffer"};
    mBufferTexture11->SetPrivateData(
      WKPDID_D3DDebugObjectName, name.size(), name.data());
  }

  D3D11_RENDER_TARGET_VIEW_DESC rtvd {
    .Format = format,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };
  winrt::check_hresult(deviceResources.mDevice11->CreateRenderTargetView(
    mBufferTexture11 ? mBufferTexture11.get() : mTexture11.get(),
    &rtvd,
    mRenderTargetView.put()));
}

RenderTargetViewFactory::~RenderTargetViewFactory() = default;

std::unique_ptr<D3D11::IRenderTargetView> RenderTargetViewFactory::Get() const {
  return std::make_unique<RenderTargetView>(
    mDeviceResources,
    mTexture12,
    mTexture11,
    mBufferTexture11,
    mRenderTargetView);
}

}// namespace OpenKneeboard::D3D11On12
