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

#include <d2d1_2.h>
#include <shims/winrt.h>

#include <functional>
#include <mutex>

namespace OpenKneeboard {

struct DXResources;

class CachedLayer final {
 public:
  CachedLayer(const DXResources&);
  ~CachedLayer();

  void Render(
    const D2D1_RECT_F& where,
    const D2D1_SIZE_U& nativeSize,
    uint16_t cacheKey,
    ID2D1DeviceContext* ctx,
    std::function<void(ID2D1DeviceContext*, const D2D1_SIZE_U&)> impl);
  void Reset();

 private:
  uint16_t mKey = ~0ui16;

  winrt::com_ptr<ID2D1DeviceContext> mCacheContext;
  D2D1_SIZE_U mCacheSize;
  winrt::com_ptr<ID2D1Bitmap1> mCache;
  std::mutex mCacheMutex;
};

}// namespace OpenKneeboard
