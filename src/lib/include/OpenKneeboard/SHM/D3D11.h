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

#include <d3d11_4.h>

namespace OpenKneeboard::SHM::D3D11 {

class Texture final : public SHM::IPCClientTexture {
 public:
  Texture() = delete;
  Texture(
    const PixelSize&,
    uint8_t swapchainIndex,
    const winrt::com_ptr<ID3D11Device5>&,
    const winrt::com_ptr<ID3D11DeviceContext4>&);
  virtual ~Texture();

  ID3D11Texture2D* GetD3D11Texture() const noexcept;
  ID3D11ShaderResourceView* GetD3D11ShaderResourceView() noexcept;

  void CopyFrom(
    ID3D11Texture2D* texture,
    ID3D11Fence* fenceIn,
    uint64_t fenceInValue,
    ID3D11Fence* fenceOut,
    uint64_t fenceOutValue) noexcept;

 private:
  winrt::com_ptr<ID3D11Device5> mDevice;
  winrt::com_ptr<ID3D11DeviceContext4> mContext;

  winrt::com_ptr<ID3D11Texture2D> mCacheTexture;
  winrt::com_ptr<ID3D11ShaderResourceView> mCacheShaderResourceView;
};

class CachedReader : public SHM::CachedReader, protected SHM::IPCTextureCopier {
 public:
  CachedReader() = delete;
  CachedReader(ConsumerKind);
  virtual ~CachedReader();

  void InitializeCache(ID3D11Device*, uint8_t swapchainLength);

 protected:
  virtual void Copy(
    HANDLE sourceTexture,
    IPCClientTexture* destinationTexture,
    HANDLE fence,
    uint64_t fenceValueIn) noexcept override;

  virtual std::shared_ptr<SHM::IPCClientTexture> CreateIPCClientTexture(
    const PixelSize&,
    uint8_t swapchainIndex) noexcept override;

  virtual void ReleaseIPCHandles() override;
  void WaitForPendingCopies();

  winrt::com_ptr<ID3D11Device5> mDevice;
  winrt::com_ptr<ID3D11DeviceContext4> mDeviceContext;
  uint64_t mDeviceLUID;

  struct FenceAndValue {
    winrt::com_ptr<ID3D11Fence> mFence;
    uint64_t mValue {};

    inline operator bool() const noexcept {
      return mFence && mValue;
    }
  };

  std::unordered_map<HANDLE, FenceAndValue> mIPCFences;
  std::unordered_map<HANDLE, winrt::com_ptr<ID3D11Texture2D>> mIPCTextures;
  FenceAndValue mCopyFence;

  FenceAndValue* GetIPCFence(HANDLE) noexcept;
  ID3D11Texture2D* GetIPCTexture(HANDLE) noexcept;
};

}// namespace OpenKneeboard::SHM::D3D11