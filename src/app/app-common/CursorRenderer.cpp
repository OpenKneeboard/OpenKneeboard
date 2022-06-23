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
#include <OpenKneeboard/CursorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/config.h>

namespace OpenKneeboard {

CursorRenderer::CursorRenderer(const DXResources& dxr) {
  auto& ctx = dxr.mD2DDeviceContext;
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mInnerBrush.put()));
  ctx->CreateSolidColorBrush(
    {1.0f, 1.0f, 1.0f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mOuterBrush.put()));
}

CursorRenderer::~CursorRenderer() = default;

void CursorRenderer::Render(
  ID2D1RenderTarget* ctx,
  const D2D1_POINT_2F& point,
  const D2D1_SIZE_F& scaleTo) {
  const auto cursorRadius = scaleTo.height / CursorRadiusDivisor;
  const auto cursorStroke = scaleTo.height / CursorStrokeDivisor;

  auto elipse = D2D1::Ellipse(point, cursorRadius, cursorRadius);
  ctx->DrawEllipse(elipse, mOuterBrush.get(), cursorStroke * 2);
  ctx->DrawEllipse(elipse, mInnerBrush.get(), cursorStroke);
}

}// namespace OpenKneeboard
