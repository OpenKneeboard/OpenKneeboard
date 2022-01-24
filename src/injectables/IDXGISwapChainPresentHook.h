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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include <dxgi.h>

#include <functional>
#include <memory>

namespace OpenKneeboard {

/** Hook for IDXGISwapChain::Present().
 *
 * This is called in both D3D11 and D3D12 apps; to determine which is being
 * used, check the type of `swapChain->GetDevice()` - it should be either an
 * `ID3D11Device` or `ID3D12Device`.
 */
class IDXGISwapChainPresentHook final {
 public:
  IDXGISwapChainPresentHook();
  ~IDXGISwapChainPresentHook();

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<HRESULT(
      IDXGISwapChain* this_,
      UINT syncInterval,
      UINT flags,
      const decltype(&IDXGISwapChain::Present)& next)>
      onPresent;
  };

  void InstallHook(const Callbacks& callbacks);
  void UninstallHook();

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
