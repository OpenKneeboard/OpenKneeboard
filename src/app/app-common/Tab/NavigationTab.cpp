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

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

#include <cstring>

namespace OpenKneeboard {

bool NavigationTab::Button::operator==(
  const NavigationTab::Button& other) const noexcept {
  return std::memcmp(&mRect, &other.mRect, sizeof(mRect)) == 0;
}

NavigationTab::NavigationTab(
  const DXResources& dxr,
  const std::shared_ptr<ITab>& rootTab,
  const std::vector<NavigationEntry>& entries,
  const D2D1_SIZE_U& _ignoredPreferredSize)
  : TabBase(winrt::guid {}, rootTab->GetTitle()),
    mDXR(dxr),
    mRootTab(rootTab),
    mPreferredSize({ErrorRenderWidth, ErrorRenderHeight}),
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
    VariableWidthUIFont,
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
  winrt::check_hresult(
    dwf->CreateEllipsisTrimmingSign(mTextFormat.get(), ellipsis.put()));
  DWRITE_TRIMMING trimming {
    .granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER};
  winrt::check_hresult(mTextFormat->SetTrimming(&trimming, ellipsis.get()));

  winrt::check_hresult(dwf->CreateTextFormat(
    VariableWidthUIFont,
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

  const auto size = this->GetPreferredSize({}).mPixelSize;
  const D2D1_RECT_F topRect {
    .left = padding,
    .top = 2 * padding,
    .right = (mPreferredSize.width / columns) - padding,
    .bottom = (2 * padding) + rowHeight,
  };
  auto rect = topRect;

  std::vector<Button> buttons;
  uint16_t column = 0;

  for (const auto& entry: entries) {
    buttons.push_back(
      {winrt::to_hstring(entry.mName), entry.mPageID, rect, column});

    rect.top = rect.bottom + padding;
    rect.bottom = rect.top + rowHeight;

    if (rect.bottom + padding > size.mHeight) {
      column = (column + 1) % columns;
      rect = topRect;
      if (column == 0) {
        PageID id;
        mPageIDs.push_back(id);
        mButtonTrackers[id] = ButtonTracker::Create(buttons);
        buttons.clear();
      } else {
        const auto translateX = column * (mPreferredSize.width / columns);
        rect.left += translateX;
        rect.right += translateX;
      }
    }
  }

  if (!buttons.empty()) {
    PageID id;
    mPageIDs.push_back(id);
    mButtonTrackers[id] = ButtonTracker::Create(buttons);
  }

  for (const auto& [pageID, buttonTracker]: mButtonTrackers) {
    AddEventListener(
      buttonTracker->evClicked, [this](auto ctx, const Button& button) {
        this->evPageChangeRequestedEvent.Emit(ctx, button.mPageID);
      });
  }
}

NavigationTab::~NavigationTab() {
  this->RemoveAllEventListeners();
}

std::string NavigationTab::GetGlyph() const {
  return mRootTab->GetGlyph();
}

PageIndex NavigationTab::GetPageCount() const {
  return static_cast<PageIndex>(mButtonTrackers.size());
}

PreferredSize NavigationTab::GetPreferredSize(PageID) {
  return {mPreferredSize, ScalingKind::Vector};
}

std::vector<PageID> NavigationTab::GetPageIDs() const {
  return mPageIDs;
}

void NavigationTab::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& ev,
  PageID pageID) {
  if (!mButtonTrackers.contains(pageID)) {
    return;
  }
  scope_guard repaintOnExit([&]() { evNeedsRepaintEvent.Emit(); });
  mButtonTrackers.at(pageID)->PostCursorEvent(ctx, ev);
}

bool NavigationTab::CanClearUserInput(PageID) const {
  return false;
}

bool NavigationTab::CanClearUserInput() const {
  return false;
}

void NavigationTab::ClearUserInput(PageID) {
  // nothing to do here
}

void NavigationTab::ClearUserInput() {
  // nothing to do here
}

void NavigationTab::RenderPage(
  RenderTarget* rt,
  PageID pageID,
  const D2D1_RECT_F& canvasRect) {
  auto ctx = rt->d2d();
  const auto scale
    = (canvasRect.bottom - canvasRect.top) / mPreferredSize.height;

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  ctx->FillRectangle(canvasRect, mBackgroundBrush.get());

  const D2D1_POINT_2F origin {canvasRect.left, canvasRect.top};
  const auto pageTransform = D2D1::Matrix3x2F::Translation(origin.x, origin.y)
    * D2D1::Matrix3x2F::Scale({scale, scale}, origin);
  ctx->SetTransform(pageTransform);

  const auto [hoverButton, buttons] = mButtonTrackers.at(pageID)->GetState();

  for (int i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons.at(i);
    const auto& rect = button.mRect;

    if (button == hoverButton) {
      ctx->FillRectangle(rect, mHighlightBrush.get());
      ctx->FillRectangle(mPreviewMetrics.mRects.at(i), mBackgroundBrush.get());
    } else {
      ctx->FillRectangle(rect, mInactiveBrush.get());
    }
  }

  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  mPreviewLayer.Render(
    {
      origin.x,
      origin.y,
      origin.x + (scale * mPreferredSize.width),
      origin.y + (scale * mPreferredSize.height),
    },
    mPreferredSize,
    pageID.GetTemporaryValue(),
    rt,
    std::bind_front(&NavigationTab::RenderPreviewLayer, this, pageID));

  ctx->SetTransform(pageTransform);

  std::vector<float> columnPreviewRightEdge(mRenderColumns);
  for (auto i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons.at(i);
    const auto& previewRect = mPreviewMetrics.mRects.at(i);
    auto& rightEdge = columnPreviewRightEdge.at(button.mRenderColumn);
    if (previewRect.right > rightEdge) {
      rightEdge = previewRect.right;
    }
    if (button == hoverButton) {
      ctx->DrawRectangle(
        previewRect, mHighlightBrush.get(), mPreviewMetrics.mStroke);
    } else {
      ctx->DrawRectangle(
        previewRect, mPreviewOutlineBrush.get(), mPreviewMetrics.mStroke / 2);
    }
  }

  for (const auto& button: buttons) {
    auto rect = button.mRect;
    rect.left = columnPreviewRightEdge.at(button.mRenderColumn)
      + mPreviewMetrics.mBleed;
    ctx->DrawTextW(
      button.mName.data(),
      static_cast<UINT32>(button.mName.size()),
      mTextFormat.get(),
      rect,
      mTextBrush.get(),
      D2D1_DRAW_TEXT_OPTIONS_NO_SNAP | D2D1_DRAW_TEXT_OPTIONS_CLIP);
  }

  auto pageIt = std::ranges::find(mPageIDs, pageID);
  if (pageIt == mPageIDs.end()) {
    return;
  }
  const auto pageIndex = static_cast<PageIndex>(pageIt - mPageIDs.begin());

  auto message = winrt::to_hstring(
    std::format(_("Page {} of {}"), pageIndex + 1, this->GetPageCount()));
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
  PageID pageID,
  RenderTarget* rt,
  const D2D1_SIZE_U& size) {
  auto& m = mPreviewMetrics;
  m = {};

  const auto& buttons = mButtonTrackers.at(pageID)->GetButtons();
  m.mRects.clear();
  m.mRects.resize(buttons.size());

  const auto& first = buttons.front();

  // just a little less than the padding
  m.mBleed = (first.mRect.bottom - first.mRect.top) * PaddingRatio * 0.1f;
  // arbitrary LGTM value
  m.mStroke = m.mBleed * 0.3f;
  m.mHeight = (first.mRect.bottom - first.mRect.top) + (m.mBleed * 2);

  for (auto i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons.at(i);

    const auto nativeSize
      = mRootTab->GetPreferredSize(button.mPageID).mPixelSize;
    const auto scale = m.mHeight / nativeSize.mHeight;
    const auto width = nativeSize.mWidth * scale;

    auto& rect = m.mRects.at(i);
    rect.left = button.mRect.left + m.mBleed;
    rect.top = button.mRect.top - m.mBleed;
    rect.right = rect.left + width;
    rect.bottom = button.mRect.bottom + m.mBleed;

    mRootTab->RenderPage(rt, button.mPageID, rect);
  }
}

void NavigationTab::Reload() {
}

}// namespace OpenKneeboard
