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
#include <OpenKneeboard/DXResources.h>

#include <OpenKneeboard/config.h>

#include <Unknwn.h>

#include <dwrite.h>

namespace OpenKneeboard {

struct D2DErrorRenderer::Impl final {
  winrt::com_ptr<IDWriteFactory> mDWrite;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;
};

D2DErrorRenderer::D2DErrorRenderer(const DXResources& dxr)
  : D2DErrorRenderer(dxr.mDWriteFactory.get(), dxr.mBlackBrush.get()) {
}

D2DErrorRenderer::D2DErrorRenderer(
  IDWriteFactory* dwrite,
  ID2D1SolidColorBrush* brush)
  : p(std::make_unique<Impl>()) {
  p->mDWrite.copy_from(dwrite);
  p->mTextBrush.copy_from(brush);
}

D2DErrorRenderer::~D2DErrorRenderer() {
}

void D2DErrorRenderer::Render(
  ID2D1DeviceContext* ctx,
  std::string_view utf8,
  const D2D1_RECT_F& where,
  ID2D1Brush* brush) {
  if (!brush) {
    brush = p->mTextBrush.get();
  }

  const auto canvasWidth = where.right - where.left,
             canvasHeight = where.bottom - where.top;

  auto text = winrt::to_hstring(utf8);

  winrt::com_ptr<IDWriteTextFormat> textFormat;

  winrt::check_hresult(p->mDWrite->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    canvasHeight * 0.05f,
    L"",
    textFormat.put()));

  winrt::com_ptr<IDWriteTextLayout> textLayout;
  winrt::check_hresult(p->mDWrite->CreateTextLayout(
    text.data(),
    static_cast<UINT32>(text.size()),
    textFormat.get(),
    canvasWidth,
    canvasHeight,
    textLayout.put()));
  textLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  textLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  ctx->DrawTextLayout({where.left, where.top}, textLayout.get(), brush);
}

}// namespace OpenKneeboard
