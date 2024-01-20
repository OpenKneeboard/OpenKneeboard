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

#include <OpenKneeboard/SHM.h>

#include <shims/winrt/base.h>

#include <d3d12.h>

namespace OpenKneeboard::SHM::D3D12 {

class Texture final : public SHM::IPCClientTexture {
 public:
  Texture() = delete;
  Texture(
    const winrt::com_ptr<ID3D12Device>&,
    const D3D12_CPU_DESCRIPTOR_HANDLE& shaderResourceViewCPUHandle,
    const D3D12_GPU_DESCRIPTOR_HANDLE& shaderResourceViewGPUHandle);
  virtual ~Texture();

  ID3D12Resource* GetD3D12Texture();
  D3D12_GPU_DESCRIPTOR_HANDLE GetD3D12ShaderResourceViewGPUHandle();

  void CopyFrom(
    ID3D12CommandList*,
    HANDLE texture,
    HANDLE fence,
    uint64_t fenceValueIn,
    uint64_t fenceValueOut) noexcept;

 private:
  winrt::com_ptr<ID3D12Device> mDevice;
  D3D12_CPU_DESCRIPTOR_HANDLE mShaderResourceViewCPUHandle;
  D3D12_GPU_DESCRIPTOR_HANDLE mShaderResourceViewGPUHandle;

  winrt::com_ptr<ID3D12Resource> mTexture;
  D3D12_RESOURCE_STATES mTextureState;
  bool mHaveShaderResourceView {false};

  HANDLE mSourceTextureHandle {};
  HANDLE mSourceFenceHandle {};

  winrt::com_ptr<ID3D12Resource> mSourceTexture;
  winrt::com_ptr<ID3D12Fence> mSourceFence;

  void InitializeSource(HANDLE texture, HANDLE fence) noexcept;
};

}// namespace OpenKneeboard::SHM::D3D12