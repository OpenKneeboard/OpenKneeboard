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
#include <OpenKneeboard/scope_guard.h>

#include <shims/winrt/base.h>

namespace OpenKneeboard::D3D11On12 {

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

RenderTargetView::~RenderTargetView() {
  if (!mBufferTexture11) {
    auto resources = static_cast<ID3D11Resource*>(mTexture11.get());
    mDeviceResources.m11on12->ReleaseWrappedResources(&resources, 1);
    return;
  }

  winrt::com_ptr<ID3D12CommandAllocator> allocator;
  winrt::check_hresult(mDeviceResources.mDevice12->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.put())));
  winrt::com_ptr<ID3D12GraphicsCommandList> commandList;
  winrt::check_hresult(mDeviceResources.mDevice12->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    allocator.get(),
    nullptr,
    IID_PPV_ARGS(commandList.put())));
  winrt::com_ptr<ID3D12Resource> bufferTexture12;
  winrt::check_hresult(mDeviceResources.m11on12->UnwrapUnderlyingResource(
    mBufferTexture11.get(),
    mDeviceResources.mCommandQueue12.get(),
    IID_PPV_ARGS(bufferTexture12.put())));
  const scope_guard returnResource([&]() noexcept {
    winrt::check_hresult(mDeviceResources.m11on12->ReturnUnderlyingResource(
      mBufferTexture11.get(), 0, nullptr, nullptr));
  });

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

  const auto commandLists = static_cast<ID3D12CommandList*>(commandList.get());
  mDeviceResources.mCommandQueue12->ExecuteCommandLists(1, &commandLists);
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
    mBufferTexture11 = SHM::CreateCompatibleTexture(
      deviceResources.mDevice11.get(),
      SHM::DEFAULT_D3D11_BIND_FLAGS,
      SHM::DEFAULT_D3D11_MISC_FLAGS,
      format);
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
