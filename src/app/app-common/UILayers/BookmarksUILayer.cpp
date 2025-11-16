// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/BookmarksUILayer.hpp>
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/config.hpp>

namespace OpenKneeboard {

std::shared_ptr<BookmarksUILayer> BookmarksUILayer::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* state,
  KneeboardView* view) {
  auto ret
    = std::shared_ptr<BookmarksUILayer>(new BookmarksUILayer(dxr, state, view));
  ret->Init();
  return ret;
}

BookmarksUILayer::BookmarksUILayer(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboardState,
  KneeboardView* kneeboardView)
  : mDXResources(dxr),
    mKneeboardState(kneeboardState),
    mKneeboardView(kneeboardView) {
  auto d2d = dxr->mD2DDeviceContext;
  d2d->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.8f}, D2D1::BrushProperties(), mBackgroundBrush.put());
  mTextBrush = dxr->mBlackBrush;
  d2d->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f}, D2D1::BrushProperties(), mHoverBrush.put());
  mCurrentPageStrokeBrush = dxr->mWhiteBrush;

  winrt::check_hresult(mDXResources->mD2DFactory->CreateStrokeStyle(
    D2D1_STROKE_STYLE_PROPERTIES {
      .dashStyle = D2D1_DASH_STYLE_DASH,
    },
    nullptr,
    0,
    mCurrentPageStrokeStyle.put()));
}

void BookmarksUILayer::Init() {
  // Need the shared_ptr to exist first
  AddEventListener(
    mKneeboardView->evBookmarksChangedEvent,
    {
      weak_from_this(),
      [](auto self) {
        self->mButtons = {};
        self->evNeedsRepaintEvent.Emit();
      },
    });
}

BookmarksUILayer::~BookmarksUILayer() {
  this->RemoveAllEventListeners();
}

void BookmarksUILayer::PostCursorEvent(
  const IUILayer::NextList& next,
  const Context& context,
  KneeboardViewID KneeboardViewID,
  const CursorEvent& cursorEvent) {
  if (
    cursorEvent.mSource == CursorSource::WindowPointer || !this->IsEnabled()) {
    this->PostNextCursorEvent(next, context, KneeboardViewID, cursorEvent);
    return;
  }

  this->PostNextCursorEvent(next, context, KneeboardViewID, cursorEvent);

  auto buttons = mButtons;
  if (!buttons) {
    return;
  }

  const auto metrics = this->GetMetrics(next, context);

  auto buttonsEvent = cursorEvent;
  buttonsEvent.mX *= metrics.mPreferredSize.mPixelSize.mWidth;
  buttonsEvent.mX /= metrics.mNextArea.Left();

  buttons->PostCursorEvent(KneeboardViewID, buttonsEvent);
}

IUILayer::Metrics BookmarksUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  auto [first, rest] = Split(next);

  const auto nextMetrics = first->GetMetrics(rest, context);

  if (!this->IsEnabled()) {
    auto ret = nextMetrics;
    ret.mNextArea = {
      {},
      nextMetrics.mPreferredSize.mPixelSize,
    };
    return ret;
  }

  const auto width = static_cast<uint32_t>(std::lround(
    nextMetrics.mContentArea.mSize.mHeight * (BookmarksBarPercent / 100.0f)));

  return Metrics {
    nextMetrics.mPreferredSize.Extended({static_cast<uint32_t>(width), 0}),
    {
      {width, 0},
      nextMetrics.mPreferredSize.mPixelSize,
    },
    {
      {
        width + nextMetrics.mContentArea.Left(),
        nextMetrics.mContentArea.Top(),
      },
      nextMetrics.mContentArea.mSize,
    },
  };
}

task<void> BookmarksUILayer::Render(
  const RenderContext& rc,
  const IUILayer::NextList& next,
  const Context& context,
  const PixelRect& rect) {
  OPENKNEEBOARD_TraceLoggingScope("BookmarksUILayer::Render()");
  auto [first, rest] = Split(next);

  if (!this->IsEnabled()) {
    co_await first->Render(rc, rest, context, rect);
    co_return;
  }

  const auto metrics = this->GetMetrics(next, context);
  const auto scale
    = rect.Width<float>() / metrics.mPreferredSize.mPixelSize.mWidth;

  auto d2d = rc.d2d();
  d2d->FillRectangle(
    PixelRect {
      rect.mOffset,
      {
        static_cast<uint32_t>(std::lround(metrics.mNextArea.Left() * scale)),
        rect.Height(),
      },
    },
    mBackgroundBrush.get());

  auto [hoverButton, buttons] = this->LayoutButtons()->GetState();

  const auto height = metrics.mPreferredSize.mPixelSize.mHeight;
  const auto width = metrics.mNextArea.Left();

  FLOAT dpix {}, dpiy {};
  d2d->GetDpi(&dpix, &dpiy);
  const auto textHeight = width * scale * 0.75;
  winrt::com_ptr<IDWriteTextFormat> textFormat;
  winrt::check_hresult(mDXResources->mDWriteFactory->CreateTextFormat(
    FixedWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_REGULAR,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (textHeight * 96) / (2 * dpiy),
    L"",
    textFormat.put()));
  textFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
  textFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
  textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

  size_t buttonNumber = 0;
  const auto currentTabView = mKneeboardView->GetCurrentTabView();
  const auto currentTab = currentTabView->GetTab().lock();
  if (!currentTab) {
    co_return;
  }

  const auto currentTabID = currentTab->GetRuntimeID();
  const auto currentPageID = currentTabView->GetPageID();
  for (const auto& button: buttons) {
    const D2D1_RECT_F buttonRect {
      rect.Left<float>(),
      rect.Top() + (button.mRect.top * height * scale),
      rect.Left() + (width * scale),
      rect.Top() + (button.mRect.bottom * height * scale),
    };
    buttonNumber++;
    const auto text = winrt::to_hstring(
      button.mBookmark.mTitle.empty() ? std::format(_("#{}"), buttonNumber)
                                      : button.mBookmark.mTitle);

    auto textBrush = (button == hoverButton) ? mHoverBrush : mTextBrush;

    d2d->DrawTextW(
      text.data(), text.size(), textFormat.get(), buttonRect, textBrush.get());
    if (
      button.mBookmark.mTabID == currentTabID
      && button.mBookmark.mPageID == currentPageID) {
      d2d->DrawLine(
        {buttonRect.right - 5, buttonRect.top + 1},
        {buttonRect.right - 5, buttonRect.bottom},
        mCurrentPageStrokeBrush.get(),
        4.0f,
        mCurrentPageStrokeStyle.get());
    }
    if (buttonNumber != 1) {
      d2d->DrawLine(
        {rect.Left<FLOAT>(), buttonRect.top},
        {rect.Left<FLOAT>() + buttonRect.right, buttonRect.top},
        mTextBrush.get(),
        2.0f);
    }
  }

  d2d.Release();
  auto nextArea
    = (metrics.mNextArea.StaticCast<float>() * scale).Rounded<uint32_t>();
  nextArea.mOffset += rect.mOffset;
  co_await first->Render(rc, rest, context, nextArea);
  d2d.Reacquire();

  d2d->DrawLine(
    {rect.Left() + (width * scale), rect.Top<float>()},
    {rect.Left() + (width * scale), rect.Bottom<float>()},
    mTextBrush.get(),
    2.0f);
}

bool BookmarksUILayer::IsEnabled() const {
  return mKneeboardState->GetUISettings().mBookmarks.mEnabled
    && !mKneeboardView->GetBookmarks().empty();
}

bool BookmarksUILayer::Button::operator==(const Button& other) const noexcept {
  return (memcmp(&mRect, &other.mRect, sizeof(mRect)) == 0)
    && (mBookmark == mBookmark);
}

BookmarksUILayer::Buttons BookmarksUILayer::LayoutButtons() {
  if (auto buttons = mButtons) {
    return buttons;
  }

  auto bookmarks = mKneeboardView->GetBookmarks();

  std::vector<Button> buttons;
  const auto interval = 1.0f / bookmarks.size();

  for (const auto& bookmark: bookmarks) {
    buttons.push_back({
      {0.0f, interval * buttons.size(), 1.0f, interval * (buttons.size() + 1)},
      bookmark,
    });
  }

  auto clickableButtons = CursorClickableRegions<Button>::Create(buttons);
  mButtons = clickableButtons;
  AddEventListener(
    clickableButtons->evClicked,
    [weak = this->weak_from_this()](auto, auto button) {
      if (auto strong = weak.lock()) {
        strong->OnClick(button);
      }
    });
  return clickableButtons;
}

void BookmarksUILayer::OnClick(const Button& button) {
  const auto tab = mKneeboardView->GetCurrentTabView()->GetRootTab().lock();
  if ((!tab) || (tab->GetRuntimeID() != button.mBookmark.mTabID)) {
    mKneeboardView->SetCurrentTabByRuntimeID(button.mBookmark.mTabID);
  }
  auto tabView = mKneeboardView->GetCurrentTabView();
  tabView->SetTabMode(TabMode::Normal);
  tabView->SetPageID(button.mBookmark.mPageID);
}

}// namespace OpenKneeboard
