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

void Texture::InitializeSource(
  HANDLE textureHandle,
  HANDLE fenceHandle) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::Texture::InitializeSource()");

  if (
    textureHandle == mSourceTextureHandle
    && fenceHandle == mSourceFenceHandle) {
    return;
  }

  if (textureHandle != mSourceTextureHandle) {
    mSourceTextureHandle = {};
    mSourceTexture = {};
    mCacheTexture = {};
    mCacheShaderResourceView = {};
  }

  if (fenceHandle != mSourceFenceHandle) {
    mSourceFenceHandle = {};
    mSourceFence = {};
  }

  if (!mSourceTexture) {
    OPENKNEEBOARD_TraceLoggingScope("OpenTexture");
    winrt::check_hresult(mDevice->OpenSharedResource1(
      textureHandle, IID_PPV_ARGS(mSourceTexture.put())));

    D3D11_TEXTURE2D_DESC desc;
    mSourceTexture->GetDesc(&desc);
    winrt::check_hresult(
      mDevice->CreateTexture2D(&desc, nullptr, mCacheTexture.put()));

    mSourceTextureHandle = textureHandle;
  }

  if (!mSourceFence) {
    OPENKNEEBOARD_TraceLoggingScope("OpenFence");
    winrt::check_hresult(
      mDevice->OpenSharedFence(fenceHandle, IID_PPV_ARGS(mSourceFence.put())));
  }
}

void Texture::CopyFrom(
  HANDLE sourceTexture,
  HANDLE sourceFence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "SHM::D3D11::Texture::CopyFrom");
  this->InitializeSource(sourceTexture, sourceFence);
  TraceLoggingWriteTagged(
    activity, "FenceIn", TraceLoggingValue(fenceValueIn, "Value"));
  winrt::check_hresult(mContext->Wait(mSourceFence.get(), fenceValueIn));
  mContext->CopySubresourceRegion(
    mCacheTexture.get(), 0, 0, 0, 0, mSourceTexture.get(), 0, nullptr);
  TraceLoggingWriteTagged(
    activity, "FenceOut", TraceLoggingValue(fenceValueOut, "Value"));
  winrt::check_hresult(mContext->Signal(mSourceFence.get(), fenceValueOut));
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

  if (device != mD3D11Device.get()) {
    mD3D11Device = {};
    mD3D11DeviceContext = {};

    winrt::check_hresult(device->QueryInterface(mD3D11Device.put()));
    winrt::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    mD3D11DeviceContext = context.as<ID3D11DeviceContext4>();

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
  HANDLE sourceTexture,
  IPCClientTexture* destinationTexture,
  HANDLE fence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::Copy()");
  reinterpret_cast<SHM::D3D11::Texture*>(destinationTexture)
    ->CopyFrom(sourceTexture, fence, fenceValueIn, fenceValueOut);
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  const PixelSize& dimensions,
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<SHM::D3D11::Texture>(
    dimensions, swapchainIndex, mD3D11Device, mD3D11DeviceContext);
}

}// namespace OpenKneeboard::SHM::D3D11