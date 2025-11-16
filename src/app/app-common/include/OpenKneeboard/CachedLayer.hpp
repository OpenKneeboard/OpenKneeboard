// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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
