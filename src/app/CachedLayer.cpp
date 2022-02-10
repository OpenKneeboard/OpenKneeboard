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
#include "CachedLayer.h"

#include <OpenKneeboard/DXResources.h>

namespace OpenKneeboard {

CachedLayer::CachedLayer(const DXResources& dxr) {
  winrt::check_hresult(dxr.mD2DDevice->CreateDeviceContext(
    D2D1_DEVICE_CONTEXT_OPTIONS_NONE, mCacheContext.put()));
}

CachedLayer::~CachedLayer() {
}

void CachedLayer::Render(
  const D2D1_RECT_F& where,
  const D2D1_SIZE_U& nativeSize,
  uint16_t cacheKey,
  ID2D1DeviceContext* ctx,
  std::function<void(ID2D1DeviceContext*, const D2D1_SIZE_U&)> impl) {
  if (mKey == cacheKey) {
    ctx->DrawBitmap(mCache.get(), where);
    return;
  }

  if (mCacheSize != nativeSize || !mCache) {
    mCache = nullptr;
    mCacheSize = nativeSize;
    D2D1_BITMAP_PROPERTIES1 props {
      .pixelFormat
      = {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
      .bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET,
    };
    winrt::check_hresult(ctx->CreateBitmap(nativeSize, nullptr, 0, &props, mCache.put()));
  }

  mCacheContext->SetTarget(mCache.get());
  mCacheContext->BeginDraw();
  mCacheContext->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
  impl(mCacheContext.get(), nativeSize);
  winrt::check_hresult(mCacheContext->EndDraw());

  ctx->DrawBitmap(mCache.get(), where);
  mKey = cacheKey;
}

void CachedLayer::Reset() {
  mKey = ~0ui16;
  mCache = nullptr;
}

}// namespace OpenKneeboard
