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
#include <OpenKneeboard/SHM/D3D11.h>

#include <OpenKneeboard/scope_guard.h>

namespace OpenKneeboard::SHM::D3D11 {

Texture::Texture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex,
  const winrt::com_ptr<ID3D11Device5>& device,
  const winrt::com_ptr<ID3D11DeviceContext4>& context)
  : IPCClientTexture(dimensions, swapchainIndex),
    mDevice(device),
    mContext(context) {
}

Texture::~Texture() = default;

ID3D11Texture2D* Texture::GetD3D11Texture() const noexcept {
  return mCacheTexture.get();
}

ID3D11ShaderResourceView* Texture::GetD3D11ShaderResourceView() noexcept {
  if (!mCacheShaderResourceView) {
    winrt::check_hresult(mDevice->CreateShaderResourceView(
      mCacheTexture.get(), nullptr, mCacheShaderResourceView.put()));
  }
  return mCacheShaderResourceView.get();
}

void Texture::CopyFrom(
  ID3D11Texture2D* sourceTexture,
  ID3D11Fence* sourceFence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::Texture::CopyFrom");

  if (!mCacheTexture) {
    OPENKNEEBOARD_TraceLoggingScope("CreateCacheTexture");
    D3D11_TEXTURE2D_DESC desc;
    sourceTexture->GetDesc(&desc);
    winrt::check_hresult(
      mDevice->CreateTexture2D(&desc, nullptr, mCacheTexture.put()));
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("FenceIn");
    winrt::check_hresult(mContext->Wait(sourceFence, fenceValueIn));
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("CopySubresourceRegion");
    mContext->CopySubresourceRegion(
      mCacheTexture.get(), 0, 0, 0, 0, sourceTexture, 0, nullptr);
  }

  {
    OPENKNEEBOARD_TraceLoggingScope("FenceOut");
    winrt::check_hresult(mContext->Signal(sourceFence, fenceValueOut));
  }
}

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::~CachedReader()");
}

void CachedReader::InitializeCache(
  ID3D11Device* device,
  uint8_t swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::InitializeCache()",
    TraceLoggingValue(swapchainLength, "swapchainLength"));

  if (device != mDevice.get()) {
    mDevice = {};
    mDeviceContext = {};

    winrt::check_hresult(device->QueryInterface(mDevice.put()));
    winrt::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    mDeviceContext = context.as<ID3D11DeviceContext4>();

    // Debug logging
    winrt::com_ptr<IDXGIDevice> dxgiDevice;
    winrt::check_hresult(
      device->QueryInterface(IID_PPV_ARGS(dxgiDevice.put())));
    winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
    winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));
    DXGI_ADAPTER_DESC desc {};
    winrt::check_hresult(dxgiAdapter->GetDesc(&desc));
    dprintf(
      L"SHM reader using adapter '{}' (LUID {:#x})",
      desc.Description,
      std::bit_cast<uint64_t>(desc.AdapterLuid));
  }

  SHM::CachedReader::InitializeCache(swapchainLength);
}

void CachedReader::Copy(
  HANDLE sourceHandle,
  IPCClientTexture* destinationTexture,
  HANDLE fenceHandle,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::Copy()");

  const auto source = this->GetIPCTexture(sourceHandle);
  const auto fence = this->GetIPCFence(fenceHandle);

  reinterpret_cast<SHM::D3D11::Texture*>(destinationTexture)
    ->CopyFrom(source, fence, fenceValueIn, fenceValueOut);
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<SHM::D3D11::Texture>(
    dimensions, swapchainIndex, mDevice, mDeviceContext);
}

ID3D11Fence* CachedReader::GetIPCFence(HANDLE handle) noexcept {
  if (mIPCFences.contains(handle)) {
    return mIPCFences.at(handle).get();
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::GetIPCFence()");
  winrt::com_ptr<ID3D11Fence> fence;
  winrt::check_hresult(
    mDevice->OpenSharedFence(handle, IID_PPV_ARGS(fence.put())));
  mIPCFences.emplace(handle, fence);
  return fence.get();
}

ID3D11Texture2D* CachedReader::GetIPCTexture(HANDLE handle) noexcept {
  if (mIPCTextures.contains(handle)) {
    return mIPCTextures.at(handle).get();
  }

  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::GetIPCTexture()");

  winrt::com_ptr<ID3D11Texture2D> texture;
  winrt::check_hresult(
    mDevice->OpenSharedResource1(handle, IID_PPV_ARGS(texture.put())));
  mIPCTextures.emplace(handle, texture);
  return texture.get();
}

}// namespace OpenKneeboard::SHM::D3D11