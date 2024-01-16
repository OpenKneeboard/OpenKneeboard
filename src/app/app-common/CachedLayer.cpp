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
#include <OpenKneeboard/CachedLayer.h>
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/SHM.h>

#include <OpenKneeboard/scope_guard.h>

#include <DirectXColors.h>

namespace OpenKneeboard {

CachedLayer::CachedLayer(const DXResources& dxr) : mDXR(dxr) {
}

CachedLayer::~CachedLayer() {
}

void CachedLayer::Render(
  const D2D1_RECT_F& where,
  Key cacheKey,
  RenderTarget* rt,
  std::function<void(RenderTarget*, const D2D1_SIZE_U&)> impl) {
  std::scoped_lock lock(mCacheMutex);

  const D2D1_SIZE_U cacheSize {
    static_cast<UINT>(std::lround(where.right - where.left)),
    static_cast<UINT>(std::lround(where.bottom - where.top)),
  };

  if (mCacheSize != cacheSize || !mCache) {
    mKey = ~Key {0};
    mCache = nullptr;
    mCacheSize = cacheSize;
    D3D11_TEXTURE2D_DESC textureDesc {
      .Width = cacheSize.width,
      .Height = cacheSize.height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = SHM::SHARED_TEXTURE_PIXEL_FORMAT,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
    };
    winrt::check_hresult(
      mDXR.mD3DDevice->CreateTexture2D(&textureDesc, nullptr, mCache.put()));
    winrt::check_hresult(mDXR.mD3DDevice->CreateShaderResourceView(
      mCache.get(), nullptr, mCacheSRV.put()));
    mCacheRenderTarget = RenderTarget::Create(mDXR, mCache);
  }

  if (mKey != cacheKey) {
    {
      auto d3d = mCacheRenderTarget->d3d();
      mDXR.mD3DImmediateContext->ClearRenderTargetView(
        d3d.rtv(), DirectX::Colors::Transparent);
    }
    impl(mCacheRenderTarget.get(), cacheSize);
    mKey = cacheKey;
  }

  auto d3d = rt->d3d();
  D3D11::DrawTextureWithOpacity(
    mDXR.mD3DDevice.get(),
    mCacheSRV.get(),
    d3d.rtv(),
    {
      0,
      0,
      static_cast<LONG>(mCacheSize.width),
      static_cast<LONG>(mCacheSize.height),
    },
    {
      static_cast<LONG>(where.left),
      static_cast<LONG>(where.top),
      static_cast<LONG>(where.right),
      static_cast<LONG>(where.bottom),
    },
    1.0f);
}

void CachedLayer::Reset() {
  std::scoped_lock lock(mCacheMutex);

  mKey = ~Key {0};
  mCache = nullptr;
  mCacheRenderTarget = nullptr;
  mCacheSRV = nullptr;
}

}// namespace OpenKneeboard
