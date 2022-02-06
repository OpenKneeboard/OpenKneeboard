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

#include <OpenKneeboard/NavigationTab.h>

namespace OpenKneeboard {

NavigationTab::NavigationTab(
  const DXResources& dxr,
  const D2D1_SIZE_U& preferredSize,
  const std::vector<Entry>& entries)
  : Tab(dxr, {}), mDXR(dxr), mPreferredSize(preferredSize) {
  auto dwf = mDXR.mDWriteFactory;
  dwf->CreateTextFormat(
    L"Segoe UI",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    30.0f,
    L"",
    mTextFormat.put());

  mDXR.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f), mBackgroundBrush.put());
  mDXR.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.8f, 1.0f, 0.5f), mHighlightBrush.put());
  mDXR.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mTextBrush.put());

  winrt::com_ptr<IDWriteTextLayout> textLayout;
  dwf->CreateTextLayout(
    L"My", 2, mTextFormat.get(), 1024, 1024, textLayout.put());
  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);
  const auto rowHeight = 1.5f * metrics.height;
  const auto padding = rowHeight / 2;

  const auto size = this->GetPreferredPixelSize(0);
  const D2D1_RECT_F topRect {
    .left = padding,
    .top = padding,
    .right = preferredSize.width - padding,
    .bottom = padding + rowHeight,
  };
  auto rect = topRect;
  mEntries.push_back({});

  auto pageEntries = &mEntries.at(0);

  for (const auto& entry: entries) {
    pageEntries->push_back({
      entry.mName,
      entry.mPageIndex,
      rect,
    });

    rect.top = rect.bottom + padding;
    rect.bottom = rect.top + rowHeight;

    if (rect.bottom > size.height) {
      mEntries.push_back({});
      pageEntries = &mEntries.back();
    }
  }

  mTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  mTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

NavigationTab::~NavigationTab() {
}

uint16_t NavigationTab::GetPageCount() const {
  return mEntries.size();
}

D2D1_SIZE_U NavigationTab::GetPreferredPixelSize(uint16_t) {
  return mPreferredSize;
}

void NavigationTab::PostCursorEvent(const CursorEvent& ev, uint16_t pageIndex) {
  mCursorPoint = {ev.mX, ev.mY};
}

void NavigationTab::RenderPageContent(
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  auto ctx = mDXR.mD2DDeviceContext;
  const auto scale = (rect.bottom - rect.top) / mPreferredSize.height;

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(rect, mBackgroundBrush.get());

  ctx->SetTransform(D2D1::Matrix3x2F::Scale({scale, scale}));

  const auto x = mCursorPoint.x;
  const auto y = mCursorPoint.y;

  for (const auto& entry: mEntries.at(pageIndex)) {
    const auto& rect = entry.mRect;
    if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom) {
      ctx->FillRectangle(rect, mHighlightBrush.get());
    }
    ctx->DrawTextW(
      entry.mName.c_str(),
      entry.mName.length(),
      mTextFormat.get(),
      rect,
      mTextBrush.get(),
      D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | D2D1_DRAW_TEXT_OPTIONS_CLIP);
  }
}

}// namespace OpenKneeboard
