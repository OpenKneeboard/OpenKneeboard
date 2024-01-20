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
#include <OpenKneeboard/SHM/D3D12.h>

#include <Windows.h>

#include <DirectXColors.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

namespace OpenKneeboard::SHM::D3D12 {

Texture::Texture(
  const winrt::com_ptr<ID3D12Device>& device,
  const D3D12_CPU_DESCRIPTOR_HANDLE& shaderResourceViewCPUHandle,
  const D3D12_GPU_DESCRIPTOR_HANDLE& shaderResourceViewGPUHandle)
  : mDevice(device),
    mShaderResourceViewCPUHandle(shaderResourceViewCPUHandle),
    mShaderResourceViewGPUHandle(shaderResourceViewGPUHandle) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::Texture()");
}

Texture::~Texture() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::~Texture()");
}

ID3D12Resource* Texture::GetD3D12Texture() {
  return mTexture.get();
}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetD3D12ShaderResourceViewGPUHandle() {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D12::Texture::GetD3D12ShaderResourceViewGPUHandle()");
  if (mHaveShaderResourceView) {
    return mShaderResourceViewGPUHandle;
  }

  if (!mTexture) [[unlikely]] {
    OPENKNEEBOARD_LOG_AND_FATAL("Can't create an SRV without a texture");
  }

  mDevice->CreateShaderResourceView(
    mTexture.get(), nullptr, mShaderResourceViewCPUHandle);
  mHaveShaderResourceView = true;

  return mShaderResourceViewGPUHandle;
}

void Texture::InitializeSource(
  HANDLE textureHandle,
  HANDLE fenceHandle) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D12::Texture::InitializeSource()");

  if (
    textureHandle == mSourceTextureHandle
    && fenceHandle == mSourceFenceHandle) {
    return;
  }

  if (textureHandle != mSourceTextureHandle) {
    mSourceTextureHandle = {};
    mSourceTexture = {};
    mTexture = {};
    mHaveShaderResourceView = false;
  }

  if (fenceHandle != mSourceFenceHandle) {
    mSourceFenceHandle = {};
    mSourceFence = {};
  }

  if (!mSourceTexture) {
    winrt::check_hresult(mDevice->OpenSharedHandle(
      textureHandle, IID_PPV_ARGS(mSourceTexture.put())));

    const D3D12_HEAP_PROPERTIES heap {D3D12_HEAP_TYPE_DEFAULT};
    auto desc = mSourceTexture->GetDesc();
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

    const D3D12_CLEAR_VALUE clearValue {
      .Format = desc.Format,
      .Color = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    winrt::check_hresult(mDevice->CreateCommittedResource(
      &heap,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      &clearValue,
      IID_PPV_ARGS(mTexture.put())));
    mTextureState = D3D12_RESOURCE_STATE_COPY_DEST;

    mSourceTextureHandle = textureHandle;
  }

  if (!mSourceFence) {
    winrt::check_hresult(
      mDevice->OpenSharedHandle(fenceHandle, IID_PPV_ARGS(mSourceFence.put())));
    mSourceFenceHandle = fenceHandle;
  }
}

}// namespace OpenKneeboard::SHM::D3D12