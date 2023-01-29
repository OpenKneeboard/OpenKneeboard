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

#include <memory>

#include "IDXGISwapChainPresentHook.h"

namespace OpenKneeboard {

class NonVRD3D11Kneeboard final {
 public:
  NonVRD3D11Kneeboard();
  virtual ~NonVRD3D11Kneeboard();

  void UninstallHook();

 private:
  SHM::Reader mSHM;
  IDXGISwapChainPresentHook mDXGIHook;

  size_t mCacheKey {};
  bool mHaveLayer = false;
  FlatConfig mFlatConfig {};
  SHM::LayerConfig mLayerConfig {};
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID3D11ShaderResourceView> mShaderResourceView;

  HRESULT OnIDXGISwapChain_Present(
    IDXGISwapChain* this_,
    UINT syncInterval,
    UINT flags,
    const decltype(&IDXGISwapChain::Present)& next);
  void CreateTexture(const winrt::com_ptr<ID3D11Device>&);
};

}// namespace OpenKneeboard
