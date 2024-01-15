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
#include <OpenKneeboard/SHM/GFXInterop.h>

namespace OpenKneeboard::SHM::GFXInterop {

LayerTextureCache::~LayerTextureCache() = default;

HANDLE LayerTextureCache::GetNTHandle() {
  if (!mNTHandle) [[unlikely]] {
    auto texture = this->GetD3D11Texture();

    winrt::com_ptr<IDXGIResource1> resource;
    winrt::check_hresult(texture->QueryInterface(resource.put()));
    winrt::check_hresult(resource->CreateSharedHandle(
      nullptr,
      DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
      nullptr,
      mNTHandle.put()));
  }
  return mNTHandle.get();
}

}// namespace OpenKneeboard::SHM::GFXInterop