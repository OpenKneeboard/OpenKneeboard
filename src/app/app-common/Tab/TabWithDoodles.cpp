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
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/DoodleRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabWithDoodles.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

namespace OpenKneeboard {

TabWithDoodles::TabWithDoodles(const DXResources& dxr, KneeboardState* kbs)
  : mContentLayer(dxr), mDXR(dxr), mKneeboard(kbs) {
  mDoodleRenderer = std::make_unique<DoodleRenderer>(dxr, kbs);

  AddEventListener(this->evContentChangedEvent, [this]() {
    this->ClearContentCache();
    this->ClearDoodles();
  });
  AddEventListener(
    mDoodleRenderer->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
}

TabWithDoodles::~TabWithDoodles() {
  this->RemoveAllEventListeners();
}

void TabWithDoodles::ClearContentCache() {
  mContentLayer.Reset();
}

void TabWithDoodles::ClearDoodles() {
  mDoodleRenderer->Clear();
}

void TabWithDoodles::PostCursorEvent(
  EventContext ctx,
  const CursorEvent& event,
  uint16_t pageIndex) {
  mDoodleRenderer->PostCursorEvent(
    ctx, event, pageIndex, this->GetNativeContentSize(pageIndex));
}

void TabWithDoodles::RenderPage(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  const auto nativeSize = this->GetNativeContentSize(pageIndex);
  mContentLayer.Render(
    rect,
    nativeSize,
    pageIndex,
    ctx,
    [&](ID2D1DeviceContext* ctx, const D2D1_SIZE_U& size) {
      this->RenderPageContent(
        ctx,
        pageIndex,
        {0.0f,
         0.f,
         static_cast<FLOAT>(size.width),
         static_cast<FLOAT>(size.height)});
    });

  mDoodleRenderer->Render(ctx, pageIndex, rect);

  this->RenderOverDoodles(ctx, pageIndex, rect);
}

void TabWithDoodles::RenderOverDoodles(
  ID2D1DeviceContext*,
  uint16_t pageIndex,
  const D2D1_RECT_F&) {
}

KneeboardState* TabWithDoodles::GetKneeboardState() {
  return mKneeboard;
}

}// namespace OpenKneeboard
