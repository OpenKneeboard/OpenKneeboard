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
#include <OpenKneeboard/BookmarksUILayer.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/KneeboardState.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/weak_wrap.h>

namespace OpenKneeboard {

std::shared_ptr<BookmarksUILayer> BookmarksUILayer::Create(
  const DXResources& dxr,
  KneeboardState* state,
  IKneeboardView* view) {
  auto ret
    = std::shared_ptr<BookmarksUILayer>(new BookmarksUILayer(dxr, state, view));
  ret->Init();
  return ret;
}

BookmarksUILayer::BookmarksUILayer(
  const DXResources& dxr,
  KneeboardState* kneeboardState,
  IKneeboardView* kneeboardView)
  : mDXResources(dxr),
    mKneeboardState(kneeboardState),
    mKneeboardView(kneeboardView) {
  auto d2d = dxr.mD2DDeviceContext;
  d2d->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.8f}, D2D1::BrushProperties(), mBackgroundBrush.put());
  d2d->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f}, D2D1::BrushProperties(), mTextBrush.put());
  d2d->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f}, D2D1::BrushProperties(), mHoverBrush.put());
}

void BookmarksUILayer::Init() {
  // Need the shared_ptr to exist first
  AddEventListener(
    mKneeboardView->evBookmarksChangedEvent, weak_wrap(this)([](auto self) {
      self->mButtons = {};
      self->evNeedsRepaintEvent.Emit();
    }));
}

BookmarksUILayer::~BookmarksUILayer() {
  this->RemoveAllEventListeners();
}

void BookmarksUILayer::PostCursorEvent(
  const IUILayer::NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  if (
    cursorEvent.mSource == CursorSource::WINDOW_POINTER || !this->IsEnabled()) {
    this->PostNextCursorEvent(next, context, eventContext, cursorEvent);
    return;
  }

  this->PostNextCursorEvent(next, context, eventContext, cursorEvent);

  auto buttons = mButtons;
  if (!buttons) {
    return;
  }

  const auto metrics = this->GetMetrics(next, context);

  auto buttonsEvent = cursorEvent;
  buttonsEvent.mX *= metrics.mCanvasSize.width;
  buttonsEvent.mY *= metrics.mCanvasSize.height;

  buttonsEvent.mX /= metrics.mNextArea.left;
  buttonsEvent.mY /= metrics.mCanvasSize.height;

  buttons->PostCursorEvent(eventContext, buttonsEvent);
}

IUILayer::Metrics BookmarksUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  auto [first, rest] = Split(next);

  const auto nextMetrics = first->GetMetrics(rest, context);

  if (!this->IsEnabled()) {
    auto ret = nextMetrics;
    ret.mNextArea = {
      0,
      0,
      nextMetrics.mCanvasSize.width,
      nextMetrics.mCanvasSize.height,
    };
    return ret;
  }

  const auto width
    = (nextMetrics.mContentArea.bottom - nextMetrics.mContentArea.top)
    * (BookmarksBarPercent / 100.0f);

  return {
    .mCanvasSize = {
      nextMetrics.mCanvasSize.width + width,
      nextMetrics.mCanvasSize.height,
    },
    .mNextArea = {
      width,
      0,
      nextMetrics.mCanvasSize.width + width,
      nextMetrics.mCanvasSize.height,
    },
    .mContentArea = {
      width + nextMetrics.mContentArea.left,
      nextMetrics.mContentArea.top,
      width + nextMetrics.mContentArea.right,
      nextMetrics.mContentArea.bottom,
    },
    .mScalingKind = nextMetrics.mScalingKind,
  };
}

void BookmarksUILayer::Render(
  RenderTargetID rtid,
  const IUILayer::NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  auto [first, rest] = Split(next);

  if (!this->IsEnabled()) {
    first->Render(rtid, rest, context, d2d, rect);
    return;
  }

  const auto metrics = this->GetMetrics(next, context);
  const auto scale = (rect.right - rect.left) / metrics.mCanvasSize.width;

  d2d->FillRectangle(
    {
      0,
      0,
      metrics.mNextArea.left * scale,
      metrics.mCanvasSize.height * scale,
    },
    mBackgroundBrush.get());

  auto [hoverButton, buttons] = this->LayoutButtons()->GetState();

  const auto height = metrics.mCanvasSize.height;
  const auto width = metrics.mNextArea.left;

  FLOAT dpix, dpiy;
  d2d->GetDpi(&dpix, &dpiy);
  const auto textHeight = width * scale * 0.75;
  winrt::com_ptr<IDWriteTextFormat> textFormat;
  winrt::check_hresult(mDXResources.mDWriteFactory->CreateTextFormat(
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
  for (const auto& button: buttons) {
    const D2D1_RECT_F buttonRect {
      rect.left,
      rect.top + (button.mRect.top * height * scale),
      rect.left + (width * scale),
      rect.top + (button.mRect.bottom * height * scale),
    };
    buttonNumber++;
    const auto text = winrt::to_hstring(
      button.mBookmark.mTitle.empty() ? std::format(_("#{}"), buttonNumber)
                                      : button.mBookmark.mTitle);

    auto textBrush = (button == hoverButton) ? mHoverBrush : mTextBrush;

    d2d->DrawTextW(
      text.data(), text.size(), textFormat.get(), buttonRect, textBrush.get());
    if (buttonNumber != 1) {
      d2d->DrawLine(
        {0, buttonRect.top},
        {buttonRect.right, buttonRect.top},
        mTextBrush.get(),
        2.0f);
    }
  }
  first->Render(
    rtid,
    rest,
    context,
    d2d,
    {
      metrics.mNextArea.left * scale,
      metrics.mNextArea.top * scale,
      metrics.mNextArea.right * scale,
      metrics.mNextArea.bottom * scale,
    });

  d2d->DrawLine(
    {width * scale, rect.top},
    {width * scale, rect.bottom},
    mTextBrush.get(),
    2.0f);
}

bool BookmarksUILayer::IsEnabled() const {
  return mKneeboardState->GetAppSettings().mBookmarks.mEnabled
    && !mKneeboardView->GetBookmarks().empty();
}

static bool operator==(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept {
  return memcmp(&a, &b, sizeof(D2D1_RECT_F)) == 0;
}

bool BookmarksUILayer::Button::operator==(const Button& other) const noexcept
  = default;

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
  if (
    mKneeboardView->GetCurrentTabView()->GetRootTab()->GetRuntimeID()
    != button.mBookmark.mTabID) {
    mKneeboardView->SetCurrentTabByRuntimeID(button.mBookmark.mTabID);
  }
  auto tabView = mKneeboardView->GetCurrentTabView();
  tabView->SetTabMode(TabMode::NORMAL);
  tabView->SetPageID(button.mBookmark.mPageID);
}

}// namespace OpenKneeboard
