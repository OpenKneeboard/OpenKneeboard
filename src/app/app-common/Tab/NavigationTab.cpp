// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/NavigationTab.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <felly/numeric_cast.hpp>

#include <cstring>

using felly::numeric_cast;

namespace OpenKneeboard {

bool NavigationTab::Button::operator==(
  const NavigationTab::Button& other) const noexcept {
  return std::memcmp(&mRect, &other.mRect, sizeof(mRect)) == 0;
}

NavigationTab::NavigationTab(
  const audited_ptr<DXResources>& dxr,
  const std::shared_ptr<ITab>& rootTab,
  const std::vector<NavigationEntry>& entries)
  : TabBase(winrt::guid {}, rootTab->GetTitle()),
    mDXR(dxr),
    mRootTab(rootTab),
    mPreferredSize(ErrorPixelSize) {
  OPENKNEEBOARD_TraceLoggingScope("NavigationTab::NavigationTab()");
  const auto columns = entries.size() >= 10
    ? std::max(
        1ui32,
        static_cast<uint32_t>(
          (mPreferredSize.mWidth * 1.5f) / mPreferredSize.mHeight))
    : 1;
  const auto entriesPerPage
    = std::min<size_t>(std::max<size_t>(20, 10 * columns), entries.size());
  const auto entriesPerColumn = entriesPerPage / columns;
  mRenderColumns = numeric_cast<uint16_t>(columns);

  auto dwf = mDXR->mDWriteFactory;
  winrt::check_hresult(dwf->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    mPreferredSize.mHeight / (3.0f * (entriesPerColumn + 1)),
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

  mDXR->mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), mBackgroundBrush.put());
  mDXR->mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.8f, 1.0f, 1.0f), mHighlightBrush.put());
  mDXR->mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f), mInactiveBrush.put());
  mDXR->mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mTextBrush.put());
  mPreviewOutlineBrush = mTextBrush;

  winrt::com_ptr<IDWriteTextLayout> textLayout;
  dwf->CreateTextLayout(
    L"My", 2, mTextFormat.get(), 1024, 1024, textLayout.put());
  DWRITE_TEXT_METRICS metrics;
  textLayout->GetMetrics(&metrics);
  const auto rowHeight = PaddingRatio * metrics.height;
  const auto padding = rowHeight / 2;

  const auto size = this->GetPreferredSize({nullptr})->mPixelSize;
  const D2D1_RECT_F topRect {
    .left = padding,
    .top = 2 * padding,
    .right = (mPreferredSize.mWidth / columns) - padding,
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
        const auto translateX = column * (mPreferredSize.mWidth / columns);
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

std::optional<PreferredSize> NavigationTab::GetPreferredSize(PageID) {
  return PreferredSize {mPreferredSize, ScalingKind::Vector};
}

std::vector<PageID> NavigationTab::GetPageIDs() const {
  return mPageIDs;
}

void NavigationTab::PostCursorEvent(
  KneeboardViewID ctx,
  const CursorEvent& ev,
  PageID pageID) {
  if (!mButtonTrackers.contains(pageID)) {
    return;
  }
  scope_exit repaintOnExit([&]() { evNeedsRepaintEvent.Emit(); });
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

task<void> NavigationTab::RenderPage(
  RenderContext rc,
  PageID pageID,
  PixelRect canvasRect) {
  OPENKNEEBOARD_TraceLoggingScope("NavigationTab::RenderPage()");
  auto ctx = rc.d2d();

  const auto scale = canvasRect.Height<float>() / mPreferredSize.mHeight;

  ctx->FillRectangle(canvasRect, mBackgroundBrush.get());

  this->CalculatePreviewMetrics(pageID);

  const auto origin = canvasRect.TopLeft();
  const auto pageTransform
    = D2D1::Matrix3x2F::Translation(origin.X<float>(), origin.Y<float>())
    * D2D1::Matrix3x2F::Scale({scale, scale}, origin);
  ctx->SetTransform(pageTransform);

  const auto [hoverButton, buttons] = mButtonTrackers.at(pageID)->GetState();

  const auto& previewMetrics = mPreviewMetrics.at(pageID);

  for (int i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons.at(i);
    const auto& rect = button.mRect;

    if (button == hoverButton) {
      ctx->FillRectangle(rect, mHighlightBrush.get());
      ctx->FillRectangle(previewMetrics.mRects.at(i), mBackgroundBrush.get());
    } else {
      ctx->FillRectangle(rect, mInactiveBrush.get());
    }
  }

  ctx.Release();

  const auto rtid = rc.GetRenderTarget()->GetID();

  if (!mPreviewCache.contains(rtid)) {
    mPreviewCache.emplace(rtid, std::make_unique<CachedLayer>(mDXR));
  }

  co_await mPreviewCache.at(rtid)->Render(
    canvasRect,
    pageID.GetTemporaryValue(),
    rc.GetRenderTarget(),
    std::bind_front(&NavigationTab::RenderPreviewLayer, this, pageID));

  ctx.Reacquire();

  ctx->SetTransform(pageTransform);

  std::vector<float> columnPreviewRightEdge(mRenderColumns);
  for (auto i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons.at(i);
    const auto& previewRect = previewMetrics.mRects.at(i);
    auto& rightEdge = columnPreviewRightEdge.at(button.mRenderColumn);
    if (previewRect.Right() > rightEdge) {
      rightEdge = previewRect.Right<FLOAT>();
    }
    if (button == hoverButton) {
      ctx->DrawRectangle(
        previewRect, mHighlightBrush.get(), previewMetrics.mStroke);
    } else {
      ctx->DrawRectangle(
        previewRect, mPreviewOutlineBrush.get(), previewMetrics.mStroke / 2);
    }
  }

  for (const auto& button: buttons) {
    auto rect = button.mRect;
    rect.left
      = columnPreviewRightEdge.at(button.mRenderColumn) + previewMetrics.mBleed;
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
    co_return;
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
     static_cast<FLOAT>(mPreferredSize.mWidth),
     static_cast<FLOAT>(mPreferredSize.mHeight) - previewMetrics.mBleed},
    mTextBrush.get(),
    D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
}

void NavigationTab::CalculatePreviewMetrics(PageID pageID) {
  if (mPreviewMetrics.contains(pageID)) {
    return;
  }
  PreviewMetrics m {};
  const auto buttons = mButtonTrackers.at(pageID)->GetButtons();

  m.mRects.resize(buttons.size());
  const auto& first = buttons.front();

  // just a little less than the padding
  m.mBleed = (first.mRect.bottom - first.mRect.top) * PaddingRatio * 0.1f;
  // arbitrary LGTM value
  m.mStroke = m.mBleed * 0.3f;

  for (auto i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons.at(i);

    const auto height
      = (button.mRect.bottom - button.mRect.top) + (2 * m.mBleed);
    const auto nativeSize
      = mRootTab->GetPreferredSize(button.mPageID)->mPixelSize;
    const auto contentScale = height / nativeSize.mHeight;

    m.mRects.at(i) = PixelRect {
      Geometry2D::Point<float>(
        button.mRect.left + m.mBleed, button.mRect.top - m.mBleed)
        .Rounded<uint32_t>(),
      Geometry2D::Size<float>(
        numeric_cast<float>(nativeSize.mWidth) * contentScale, height)
        .Rounded<uint32_t>(),
    };
  }

  mPreviewMetrics.emplace(pageID, std::move(m));
}

task<void> NavigationTab::RenderPreviewLayer(
  PageID pageID,
  RenderTarget* rt,
  const PixelSize& size) {
  OPENKNEEBOARD_TraceLoggingScope("NavigationTab::RenderPreviewLayer()");
  const auto& m = mPreviewMetrics.at(pageID);
  const auto buttons = mButtonTrackers.at(pageID)->GetButtons();

  const auto scale
    = size.Height<float>() / this->GetPreferredSize(pageID)->mPixelSize.mHeight;

  const RenderContext rc {rt, nullptr};

  for (auto i = 0; i < buttons.size(); ++i) {
    const auto& button = buttons.at(i);
    const auto& rect = m.mRects.at(i);
    const auto scaled = rect.StaticCast<float>() * scale;
    co_await mRootTab->RenderPage(
      rc, button.mPageID, scaled.Rounded<uint32_t>());
  }
}

task<void> NavigationTab::Reload() {
  co_return;
}

}// namespace OpenKneeboard
