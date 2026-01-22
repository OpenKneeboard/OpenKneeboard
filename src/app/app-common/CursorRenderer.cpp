// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/CursorRenderer.hpp>
#include <OpenKneeboard/DXResources.hpp>

#include <OpenKneeboard/config.hpp>

namespace OpenKneeboard {

CursorRenderer::CursorRenderer(const audited_ptr<DXResources>& dxr) {
  mInnerBrush = dxr->mCursorInnerBrush;
  mOuterBrush = dxr->mCursorOuterBrush;
}

CursorRenderer::~CursorRenderer() = default;

void CursorRenderer::Render(
  ID2D1RenderTarget* ctx,
  const PixelPoint& point,
  const PixelSize& scaleTo) {
  const auto cursorRadius = scaleTo.Height<float>() / CursorRadiusDivisor;
  const auto cursorStroke = scaleTo.Height<float>() / CursorStrokeDivisor;

  auto elipse = D2D1::Ellipse(point, cursorRadius, cursorRadius);
  ctx->DrawEllipse(elipse, mOuterBrush.get(), cursorStroke * 2);
  ctx->DrawEllipse(elipse, mInnerBrush.get(), cursorStroke);
}

}// namespace OpenKneeboard
