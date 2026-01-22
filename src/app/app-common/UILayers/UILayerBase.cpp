// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/UILayerBase.hpp>

namespace OpenKneeboard {

UILayerBase::~UILayerBase() = default;

void UILayerBase::PostNextCursorEvent(
  const IUILayer::NextList& next,
  const IUILayer::Context& context,
  KneeboardViewID KneeboardViewID,
  const CursorEvent& cursorEvent) {
  const auto [first, rest] = Split(next);

  if (cursorEvent.mTouchState == CursorTouchState::NotNearSurface) {
    first->PostCursorEvent(rest, context, KneeboardViewID, {});
    return;
  }

  const auto metrics = this->GetMetrics(next, context);

  CursorEvent nextEvent {cursorEvent};

  nextEvent.mX *= metrics.mPreferredSize.mPixelSize.mWidth;
  nextEvent.mY *= metrics.mPreferredSize.mPixelSize.mHeight;

  nextEvent.mX -= metrics.mNextArea.Left();
  nextEvent.mY -= metrics.mNextArea.Top();

  nextEvent.mX /= metrics.mNextArea.Width();
  nextEvent.mY /= metrics.mNextArea.Height();

  if (
    nextEvent.mX < 0 || nextEvent.mX > 1 || nextEvent.mY < 0
    || nextEvent.mY > 1) {
    first->PostCursorEvent(rest, context, KneeboardViewID, {});
  } else {
    first->PostCursorEvent(rest, context, KneeboardViewID, nextEvent);
  }
}

}// namespace OpenKneeboard
