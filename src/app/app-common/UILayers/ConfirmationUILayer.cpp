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
#include <OpenKneeboard/ConfirmationUILayer.h>
#include <OpenKneeboard/IToolbarItemWithConfirmation.h>

namespace OpenKneeboard {

ConfirmationUILayer::ConfirmationUILayer(
  const std::shared_ptr<IToolbarItemWithConfirmation>& item)
  : mItem(item) {
}

ConfirmationUILayer::~ConfirmationUILayer() = default;

void ConfirmationUILayer::PostCursorEvent(
  const NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  next.front()->PostCursorEvent(
    next.subspan(1), context, eventContext, cursorEvent);
}

void ConfirmationUILayer::Render(
  const NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  next.front()->Render(next.subspan(1), context, d2d, rect);
}

IUILayer::Metrics ConfirmationUILayer::GetMetrics(
  const NextList& next,
  const Context& context) const {
  return next.front()->GetMetrics(next.subspan(1), context);
}

}// namespace OpenKneeboard
