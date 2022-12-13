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
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/TabViewUILayer.h>
#include <OpenKneeboard/utf8.h>

namespace OpenKneeboard {

TabViewUILayer::TabViewUILayer(const DXResources& dxr) {
  mErrorRenderer
    = std::make_unique<D2DErrorRenderer>(dxr.mD2DDeviceContext.get());
}
TabViewUILayer::~TabViewUILayer() = default;

void TabViewUILayer::PostCursorEvent(
  const IUILayer::NextList&,
  const Context& context,
  const EventContext&,
  const CursorEvent& ev) {
  if (ev.mTouchState == CursorTouchState::NOT_NEAR_SURFACE) {
    mCursorPoint.reset();
    context.mTabView->PostCursorEvent({});
    return;
  }
  const auto size = context.mTabView->GetNativeContentSize();
  auto tabEvent {ev};
  tabEvent.mX *= size.width;
  tabEvent.mY *= size.height;
  context.mTabView->PostCursorEvent(tabEvent);
  mCursorPoint = {tabEvent.mX, tabEvent.mY};
}

IUILayer::Metrics TabViewUILayer::GetMetrics(
  const IUILayer::NextList&,
  const Context& context) const {
  const constexpr Metrics errorMapping {
    {768, 1024},
    {0, 0, 768, 1024},
  };
  auto tabView = context.mTabView;
  if (!tabView) {
    return errorMapping;
  }
  const auto nextSize = tabView->GetNativeContentSize();

  if (nextSize.width == 0 || nextSize.height == 0) {
    return errorMapping;
  }

  const D2D1_SIZE_F size {
    static_cast<FLOAT>(nextSize.width),
    static_cast<FLOAT>(nextSize.height),
  };

  return {
    size,
    {0, 0, size.width, size.height},
  };
}

void TabViewUILayer::Render(
  const IUILayer::NextList&,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  const auto tabView = context.mTabView;

  if (!tabView) {
    mErrorRenderer->Render(d2d, _("No Tab View"), rect);
    return;
  }
  auto tab = tabView->GetTab();
  if (!tab) {
    mErrorRenderer->Render(d2d, _("No Tab"), rect);
    return;
  }
  const auto pageCount = tab->GetPageCount();
  if (pageCount == 0) {
    mErrorRenderer->Render(d2d, _("No Pages"), rect);
    return;
  }

  const auto pageIndex = tabView->GetPageIndex();
  if (pageIndex >= pageCount) {
    mErrorRenderer->Render(d2d, _("Invalid Page Number"), rect);
    return;
  }

  tab->RenderPage(d2d, pageIndex, rect);
}

std::optional<D2D1_POINT_2F> TabViewUILayer::GetCursorPoint() const {
  return mCursorPoint;
}

}// namespace OpenKneeboard
