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
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <Unknwn.h>
#include <dwrite.h>

namespace OpenKneeboard {

// TODO: naming conventions
struct D2DErrorRenderer::Impl final {
  winrt::com_ptr<ID2D1DeviceContext> ctx;
  winrt::com_ptr<IDWriteFactory> dwrite;
  winrt::com_ptr<IDWriteTextFormat> textFormat;
  winrt::com_ptr<ID2D1SolidColorBrush> textBrush;
};

D2DErrorRenderer::D2DErrorRenderer(
  const winrt::com_ptr<ID2D1DeviceContext>& ctx)
  : p(std::make_unique<Impl>()) {
  p->ctx = ctx;

  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(p->dwrite.put()));

  p->dwrite->CreateTextFormat(
    L"Segoe UI",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    16.0f,
    L"",
    p->textFormat.put());

  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    p->textBrush.put());
}

D2DErrorRenderer::~D2DErrorRenderer() {
}

void D2DErrorRenderer::Render(
  const std::wstring& text,
  const D2D1_RECT_F& where) {
  const auto canvasWidth = where.right - where.left,
             canvasHeight = where.bottom - where.top;

  winrt::com_ptr<IDWriteTextLayout> textLayout;
  p->dwrite->CreateTextLayout(
    text.data(),
    static_cast<UINT32>(text.size()),
    p->textFormat.get(),
    canvasWidth,
    canvasHeight,
    textLayout.put());

  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);

  const auto textWidth = metrics.width, textHeight = metrics.height;

  p->ctx->DrawTextLayout(
    {where.left + ((canvasWidth - textWidth) / 2),
     where.top + ((canvasHeight - textHeight) / 2)},
    textLayout.get(),
    p->textBrush.get());
}

}// namespace OpenKneeboard
