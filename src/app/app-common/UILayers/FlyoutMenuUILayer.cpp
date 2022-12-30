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
#include <OpenKneeboard/FlyoutMenuUILayer.h>

namespace OpenKneeboard {

FlyoutMenuUILayer::FlyoutMenuUILayer(
  const DXResources& dxr,
  const std::vector<std::shared_ptr<IToolbarItem>>& items,
  D2D1_POINT_2F preferredTopLeft01,
  D2D1_POINT_2F preferredTopRight01,
  PreferredAnchor preferredAnchor)
  : mDXResources(dxr),
    mItems(items),
    mPreferredTopLeft01(preferredTopLeft01),
    mPreferredTopRight01(preferredTopRight01) {
}

FlyoutMenuUILayer::~FlyoutMenuUILayer() = default;

void FlyoutMenuUILayer::PostCursorEvent(
  const NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  next.front()->PostCursorEvent(
    next.subspan(1), context, eventContext, cursorEvent);
}

IUILayer::Metrics FlyoutMenuUILayer::GetMetrics(
  const NextList& next,
  const Context& context) const {
  return next.front()->GetMetrics(next.subspan(1), context);
}

void FlyoutMenuUILayer::Render(
  const NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  next.front()->Render(next.subspan(1), context, d2d, rect);
}

}// namespace OpenKneeboard
