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

namespace OpenKneeboard {

BookmarksUILayer::BookmarksUILayer(
  const DXResources& dxr,
  KneeboardState* kneeboardState,
  IKneeboardView* kneeboardView)
  : mKneeboardState(kneeboardState), mKneeboardView(kneeboardView) {
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

  // TODO
  this->PostNextCursorEvent(next, context, eventContext, cursorEvent);
}

IUILayer::Metrics BookmarksUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  auto [first, rest] = Split(next);

  const auto nextMetrics = first->GetMetrics(rest, context);

  if (!this->IsEnabled()) {
    return {
      .mCanvasSize = nextMetrics.mCanvasSize,
      .mNextArea = {
        0,
        0,
        nextMetrics.mCanvasSize.width,
        nextMetrics.mCanvasSize.height,
      }, 
      .mContentArea = nextMetrics.mContentArea,
    };
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
  };
}

void BookmarksUILayer::Render(
  const IUILayer::NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  auto [first, rest] = Split(next);

  if (!this->IsEnabled()) {
    first->Render(rest, context, d2d, rect);
    return;
  }

  const auto metrics = this->GetMetrics(next, context);
  const auto scale = (rect.right - rect.left) / metrics.mCanvasSize.width;

  first->Render(
    rest,
    context,
    d2d,
    {
      metrics.mNextArea.left * scale,
      metrics.mNextArea.top * scale,
      metrics.mNextArea.right * scale,
      metrics.mNextArea.bottom * scale,
    });
}

bool BookmarksUILayer::IsEnabled() const {
  return mKneeboardState->GetAppSettings().mBookmarks.mEnabled
    && !mKneeboardView->GetBookmarks().empty();
}

static bool operator==(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept {
  return memcmp(&a, &b, sizeof(D2D1_RECT_F)) == 0;
}

}// namespace OpenKneeboard
