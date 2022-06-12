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
#include <OpenKneeboard/dprint.h>

namespace OpenKneeboard {

NavigationTab::NavigationTab(
  const DXResources& dxr,
  Tab* rootTab,
  const std::vector<Entry>& entries,
  const D2D1_SIZE_U& _ignoredPreferredSize)
  : mDXR(dxr),
    mRootTab(rootTab),
    mPreferredSize({1024, 768}),
    mPreviewLayer(dxr) {
  const auto columns = entries.size() >= 10
    ? std::max(
      1ui32,
      static_cast<uint32_t>(
        (mPreferredSize.width * 1.5f) / mPreferredSize.height))
    : 1;
  const auto entriesPerPage
    = std::min<size_t>(std::max<size_t>(20, 10 * columns), entries.size());
  const auto entriesPerColumn = entriesPerPage / columns;
  mRenderColumns = columns;

  auto dwf = mDXR.mDWriteFactory;
  winrt::check_hresult(dwf->CreateTextFormat(
    L"Segoe UI",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    mPreferredSize.height / (3.0f * (entriesPerColumn + 1)),
    L"",
    mTextFormat.put()));

  mTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  mTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  winrt::com_ptr<IDWriteInlineObject> ellipsis;
  dwf->CreateEllipsisTrimmingSign(mTextFormat.get(), ellipsis.put());
  DWRITE_TRIMMING trimming {
    .granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER};
  mTextFormat->SetTrimming(&trimming, ellipsis.get());

  winrt::check_hresult(dwf->CreateTextFormat(
    L"Segoe UI",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    mTextFormat->GetFontSize() / 2,
    L"",
    mPageNumberTextFormat.put()));
  mPageNumberTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  mPageNumberTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);

  mDXR.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), mBackgroundBrush.put());
  mDXR.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.8f, 1.0f, 1.0f), mHighlightBrush.put());
  mDXR.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f), mInactiveBrush.put());
  mDXR.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mTextBrush.put());
  mPreviewOutlineBrush = mTextBrush;

  winrt::com_ptr<IDWriteTextLayout> textLayout;
  dwf->CreateTextLayout(
    L"My", 2, mTextFormat.get(), 1024, 1024, textLayout.put());
  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);
  const auto rowHeight = PaddingRatio * metrics.height;
  const auto padding = rowHeight / 2;

  const auto size = this->GetNativeContentSize(0);
  const D2D1_RECT_F topRect {
    .left = padding,
    .top = 2 * padding,
    .right = (mPreferredSize.width / columns) - padding,
    .bottom = (2 * padding) + rowHeight,
  };
  auto rect = topRect;
  mEntries.push_back({});

  auto pageEntries = &mEntries.at(0);
  uint16_t column = 0;

  for (const auto& entry: entries) {
    pageEntries->push_back(
      {winrt::to_hstring(entry.mName), entry.mPageIndex, rect, column});

    rect.top = rect.bottom + padding;
    rect.bottom = rect.top + rowHeight;

    if (rect.bottom + padding > size.height) {
      column = (column + 1) % columns;
      rect = topRect;
      if (column == 0) {
        mEntries.push_back({});
        pageEntries = &mEntries.back();
      } else {
        const auto translateX = column * (mPreferredSize.width / columns);
        rect.left += translateX;
        rect.right += translateX;
      }
    }
  }
  if (mEntries.back().empty()) {
    mEntries.pop_back();
  }
}

NavigationTab::~NavigationTab() {
}

utf8_string NavigationTab::GetTitle() const {
  return mRootTab->GetTitle();
}

utf8_string NavigationTab::GetGlyph() const {
  return mRootTab->GetGlyph();
}

uint16_t NavigationTab::GetPageCount() const {
  return static_cast<uint16_t>(mEntries.size());
}

D2D1_SIZE_U NavigationTab::GetNativeContentSize(uint16_t) {
  return mPreferredSize;
}

void NavigationTab::PostCursorEvent(const CursorEvent& ev, uint16_t pageIndex) {
  evNeedsRepaintEvent.Emit();
  mCursorPoint = {ev.mX, ev.mY};

  // We only care about touch start and end

  // moving with button pressed: no state change
  if (
    ev.mTouchState == CursorTouchState::TOUCHING_SURFACE
    && mButtonState != ButtonState::NOT_PRESSED) {
    return;
  }

  // moving without button pressed, no state change
  if (
    ev.mTouchState != CursorTouchState::TOUCHING_SURFACE
    && mButtonState == ButtonState::NOT_PRESSED) {
    return;
  }

  // touch end, but touch start wasn't on a button
  if (
    ev.mTouchState != CursorTouchState::TOUCHING_SURFACE
    && mButtonState == ButtonState::PRESSING_INACTIVE_AREA) {
    mButtonState = ButtonState::NOT_PRESSED;
    return;
  }

  bool matched = false;
  uint16_t matchedIndex;
  uint16_t matchedPage;
  const auto& entries = mEntries.at(pageIndex);
  for (uint16_t i = 0; i < entries.size(); ++i) {
    const auto& r = entries.at(i).mRect;
    if (
      ev.mX >= r.left && ev.mX <= r.right && ev.mY >= r.top
      && ev.mY <= r.bottom) {
      matched = true;
      matchedIndex = i;
      matchedPage = entries.at(i).mPageIndex;
      break;
    }
  }

  // touch start
  if (ev.mTouchState == CursorTouchState::TOUCHING_SURFACE) {
    if (matched) {
      mActiveButton = matchedIndex;
      mButtonState = ButtonState::PRESSING_BUTTON;
    } else {
      mButtonState = ButtonState::PRESSING_INACTIVE_AREA;
    }
    return;
  }

  mButtonState = ButtonState::NOT_PRESSED;

  // touch release, but not on the same button
  if ((!matched) || matchedIndex != mActiveButton) {
    return;
  }

  // touch release, on the same button
  evPageChangeRequestedEvent.Emit(matchedPage);
}

void NavigationTab::RenderPage(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& canvasRect) {
  const auto scale
    = (canvasRect.bottom - canvasRect.top) / mPreferredSize.height;

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(canvasRect, mBackgroundBrush.get());

  const D2D1_POINT_2F origin {canvasRect.left, canvasRect.top};
  ctx->SetTransform(
    D2D1::Matrix3x2F::Translation(origin.x, origin.y)
    * D2D1::Matrix3x2F::Scale({scale, scale}, origin));

  const auto& pageEntries = mEntries.at(pageIndex);

  uint16_t hoverPage = ~0ui16;
  const auto x = mCursorPoint.x;
  const auto y = mCursorPoint.y;

  for (int i = 0; i < pageEntries.size(); ++i) {
    const auto& entry = pageEntries.at(i);
    const auto& rect = entry.mRect;
    bool hover = false;

    switch (mButtonState) {
      case ButtonState::NOT_PRESSED:
        hover = x >= rect.left && x <= rect.right && y >= rect.top
          && y <= rect.bottom;
        break;
      case ButtonState::PRESSING_BUTTON:
        hover = (i == mActiveButton);
        break;
      case ButtonState::PRESSING_INACTIVE_AREA:
        break;
    }

    if (hover) {
      hoverPage = i;
      ctx->FillRectangle(rect, mHighlightBrush.get());
      ctx->FillRectangle(mPreviewMetrics.mRects.at(i), mBackgroundBrush.get());
    } else {
      ctx->FillRectangle(rect, mInactiveBrush.get());
    }
  }

  mPreviewLayer.Render(
    {0.0f,
     0.0f,
     static_cast<float>(mPreferredSize.width),
     static_cast<float>(mPreferredSize.height)},
    mPreferredSize,
    pageIndex,
    ctx,
    std::bind_front(&NavigationTab::RenderPreviewLayer, this, pageIndex));

  std::vector<float> columnPreviewRightEdge(mRenderColumns);
  for (auto i = 0; i < pageEntries.size(); ++i) {
    const auto& entry = pageEntries.at(i);
    const auto& previewRect = mPreviewMetrics.mRects.at(i);
    auto& rightEdge = columnPreviewRightEdge.at(entry.mRenderColumn);
    if (previewRect.right > rightEdge) {
      rightEdge = previewRect.right;
    }
    if (hoverPage == i) {
      ctx->DrawRectangle(
        previewRect, mHighlightBrush.get(), mPreviewMetrics.mStroke);
    } else {
      ctx->DrawRectangle(
        previewRect, mPreviewOutlineBrush.get(), mPreviewMetrics.mStroke / 2);
    }
  }

  for (const auto& entry: pageEntries) {
    auto rect = entry.mRect;
    rect.left
      = columnPreviewRightEdge.at(entry.mRenderColumn) + mPreviewMetrics.mBleed;
    ctx->DrawTextW(
      entry.mName.data(),
      static_cast<UINT32>(entry.mName.size()),
      mTextFormat.get(),
      rect,
      mTextBrush.get(),
      D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | D2D1_DRAW_TEXT_OPTIONS_CLIP);
  }

  auto message = winrt::to_hstring(fmt::format(
    fmt::runtime(_("Page {} of {}")), pageIndex + 1, mEntries.size()));
  ctx->DrawTextW(
    message.data(),
    static_cast<UINT32>(message.size()),
    mPageNumberTextFormat.get(),
    {0.0f,
     0.0f,
     static_cast<FLOAT>(mPreferredSize.width),
     static_cast<FLOAT>(mPreferredSize.height) - mPreviewMetrics.mBleed},
    mTextBrush.get(),
    D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
}

void NavigationTab::RenderPreviewLayer(
  uint16_t pageIndex,
  ID2D1DeviceContext* ctx,
  const D2D1_SIZE_U& size) {
  mPreviewMetricsPage = pageIndex;
  auto& m = mPreviewMetrics;
  m = {};

  const auto& pageEntries = mEntries.at(pageIndex);
  m.mRects.clear();
  m.mRects.resize(pageEntries.size());

  const auto& first = pageEntries.front();

  // just a little less than the padding
  m.mBleed = (first.mRect.bottom - first.mRect.top) * PaddingRatio * 0.1f;
  // arbitrary LGTM value
  m.mStroke = m.mBleed * 0.3f;
  m.mHeight = (first.mRect.bottom - first.mRect.top) + (m.mBleed * 2);

  for (auto i = 0; i < pageEntries.size(); ++i) {
    const auto& entry = pageEntries.at(i);

    const auto nativeSize = mRootTab->GetNativeContentSize(entry.mPageIndex);
    const auto scale = m.mHeight / nativeSize.height;
    const auto width = nativeSize.width * scale;

    auto& rect = m.mRects.at(i);
    rect.left = entry.mRect.left + m.mBleed;
    rect.top = entry.mRect.top - m.mBleed;
    rect.right = rect.left + width;
    rect.bottom = entry.mRect.bottom + m.mBleed;

    mRootTab->RenderPage(ctx, entry.mPageIndex, rect);
  }
}

void NavigationTab::Reload() {
}

}// namespace OpenKneeboard
