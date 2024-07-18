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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/UILayerBase.h>

namespace OpenKneeboard {

UILayerBase::~UILayerBase() = default;

void UILayerBase::PostNextCursorEvent(
  const IUILayer::NextList& next,
  const IUILayer::Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  const auto [first, rest] = Split(next);

  if (cursorEvent.mTouchState == CursorTouchState::NotNearSurface) {
    first->PostCursorEvent(rest, context, eventContext, {});
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
    first->PostCursorEvent(rest, context, eventContext, {});
  } else {
    first->PostCursorEvent(rest, context, eventContext, nextEvent);
  }
}

}// namespace OpenKneeboard
