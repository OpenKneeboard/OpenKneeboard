// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/D2DErrorRenderer.hpp>
#include <OpenKneeboard/DXResources.hpp>

#include <Unknwn.h>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/hresult.hpp>

#include <dwrite.h>

namespace OpenKneeboard {

struct D2DErrorRenderer::Impl final {
  winrt::com_ptr<IDWriteFactory> mDWrite;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;
};

D2DErrorRenderer::D2DErrorRenderer(const audited_ptr<DXResources>& dxr)
  : D2DErrorRenderer(dxr->mDWriteFactory.get(), dxr->mBlackBrush.get()) {
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

  check_hresult(p->mDWrite->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    canvasHeight * 0.05f,
    L"",
    textFormat.put()));

  winrt::com_ptr<IDWriteTextLayout> textLayout;
  check_hresult(p->mDWrite->CreateTextLayout(
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
