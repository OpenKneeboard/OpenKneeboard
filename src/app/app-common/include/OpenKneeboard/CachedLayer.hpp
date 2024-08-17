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

#include <OpenKneeboard/D3D11.hpp>
#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/RenderTarget.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/task.hpp>

#include <functional>
#include <mutex>

#include <d2d1_2.h>

namespace OpenKneeboard {

struct DXResources;

class CachedLayer final {
 public:
  using Key = size_t;
  CachedLayer() = delete;
  CachedLayer(const audited_ptr<DXResources>&);
  ~CachedLayer();

  [[nodiscard]]
  task<void> Render(
    const PixelRect& where,
    Key cacheKey,
    RenderTarget*,
    std::function<task<void>(RenderTarget*, const PixelSize&)> impl,
    const std::optional<PixelSize>& cacheDimensions = {});
  void Reset();

 private:
  Key mKey = ~Key {0};
  audited_ptr<DXResources> mDXR;

  PixelSize mCacheDimensions;
  std::mutex mCacheMutex;
  std::shared_ptr<RenderTarget> mCacheRenderTarget;
  winrt::com_ptr<ID3D11Texture2D> mCache;
  winrt::com_ptr<ID3D11ShaderResourceView> mCacheSRV;
};

}// namespace OpenKneeboard
