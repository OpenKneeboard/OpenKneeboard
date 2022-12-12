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
#include <OpenKneeboard/CursorRenderer.h>
#include <OpenKneeboard/CursorUILayer.h>

namespace OpenKneeboard {

CursorUILayer::CursorUILayer(const DXResources& dxr)
  : mRenderer(std::make_unique<CursorRenderer>(dxr)) {
}

CursorUILayer::~CursorUILayer() = default;

void CursorUILayer::PostCursorEvent(
  const IUILayer::NextList&,
  const Context&,
  const EventContext&,
  const CursorEvent& ev) {
  // FIXME: delegate

  if (ev.mTouchState == CursorTouchState::NOT_NEAR_SURFACE) {
    mCursor = {};
  } else {
    mCursor = {ev.mX, ev.mY};
  }
}

D2D1_SIZE_F
CursorUILayer::GetPreferredSize(
  const IUILayer::NextList& next,
  const Context& context) const {
  return next.front()->GetPreferredSize(next.subspan(1), context);
}

void CursorUILayer::Render(
  const IUILayer::NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  next.front()->Render(next.subspan(1), context, d2d, rect);
  if (mCursor) {
    d2d->SetTransform(D2D1::Matrix3x2F::Identity());
    mRenderer->Render(
      d2d, *mCursor, {rect.right - rect.left, rect.bottom - rect.top});
  }
}

}// namespace OpenKneeboard
