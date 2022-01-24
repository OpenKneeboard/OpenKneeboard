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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <Unknwn.h>
#include <dwrite.h>

namespace OpenKneeboard {

class D2DErrorRenderer::Impl final {
 public:
  winrt::com_ptr<ID2D1Factory> d2d;
  winrt::com_ptr<IDWriteFactory> dwrite;

  winrt::com_ptr<IDWriteTextFormat> textFormat;

  winrt::com_ptr<ID2D1RenderTarget> rt;
  struct {
    winrt::com_ptr<ID2D1Brush> textBrush;
  } rtVar;
};

D2DErrorRenderer::D2DErrorRenderer(const winrt::com_ptr<ID2D1Factory>& d2df)
  : p(std::make_unique<Impl>()) {
  p->d2d = d2df;
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
}

void D2DErrorRenderer::SetRenderTarget(
  const winrt::com_ptr<ID2D1RenderTarget>& rt) {
  if (rt == p->rt) {
    return;
  }

  p->rt = rt;
  p->rtVar = {};

  rt->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(p->rtVar.textBrush.put()));
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

  p->rt->DrawTextLayout(
    {where.left + ((canvasWidth - textWidth) / 2),
     where.top + ((canvasHeight - textHeight) / 2)},
    textLayout.get(),
    p->rtVar.textBrush.get());
}

}// namespace OpenKneeboard
