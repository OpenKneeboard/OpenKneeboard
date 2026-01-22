// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Pixels.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <shims/winrt/base.h>

#include <d2d1.h>
namespace OpenKneeboard {

struct DXResources;

class CursorRenderer final {
 public:
  CursorRenderer() = delete;
  CursorRenderer(const audited_ptr<DXResources>&);
  ~CursorRenderer();

  void Render(
    ID2D1RenderTarget* ctx,
    const PixelPoint& point,
    const PixelSize& scaleTo);

 private:
  winrt::com_ptr<ID2D1SolidColorBrush> mInnerBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mOuterBrush;
};

}// namespace OpenKneeboard
