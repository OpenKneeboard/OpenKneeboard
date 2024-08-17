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
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/D2DErrorRenderer.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabViewUILayer.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/utf8.hpp>

namespace OpenKneeboard {

TabViewUILayer::TabViewUILayer(const audited_ptr<DXResources>& dxr) {
  mErrorRenderer = std::make_unique<D2DErrorRenderer>(dxr);
  dxr->mD2DDeviceContext->CreateSolidColorBrush(
    {1.0f, 1.0f, 1.0f, 1.0f},
    D2D1::BrushProperties(),
    mErrorBackgroundBrush.put());
}
TabViewUILayer::~TabViewUILayer() = default;

void TabViewUILayer::PostCursorEvent(
  const IUILayer::NextList&,
  const Context& context,
  KneeboardViewID,
  const CursorEvent& ev) {
  if (ev.mTouchState == CursorTouchState::NotNearSurface) {
    mCursorPoint.reset();
    context.mTabView->PostCursorEvent({});
    return;
  }
  mCursorPoint = {ev.mX, ev.mY};
  context.mTabView->PostCursorEvent(ev);
}

IUILayer::Metrics TabViewUILayer::GetMetrics(
  const IUILayer::NextList&,
  const Context& context) const {
  const constexpr Metrics errorMapping {
    {ErrorRenderSize, ScalingKind::Vector},
    {{}, ErrorRenderSize},
    {{}, ErrorRenderSize},
  };
  auto tabView = context.mTabView;
  if (!tabView) {
    return errorMapping;
  }
  const auto nextSize = tabView->GetPreferredSize();
  const auto& ps = nextSize.mPixelSize;

  if (ps.mWidth == 0 || ps.mHeight == 0) {
    return errorMapping;
  }

  return {
    nextSize,
    {{}, ps},
    {{}, ps},
  };
}

task<void> TabViewUILayer::Render(
  const RenderContext& rc,
  const IUILayer::NextList&,
  const Context& context,
  const PixelRect& rect) {
  const auto tabView = context.mTabView;

  OPENKNEEBOARD_TraceLoggingScope(
    "TabViewUILayer::Render()",
    TraceLoggingHexUInt64(
      rc.GetRenderTarget()->GetID().GetTemporaryValue(), "RenderTargetID"),
    TraceLoggingGuid(
      (tabView && tabView->GetTab()) ? tabView->GetTab()->GetPersistentID()
                                     : winrt::guid {},
      "TabID"),
    TraceLoggingHexUInt64(
      tabView ? tabView->GetPageID().GetTemporaryValue() : 0ui64, "PageID"));

  if (!tabView) {
    this->RenderError(rc.d2d(), _("No Tab View"), rect);
    co_return;
  }
  auto tab = tabView->GetTab();
  if (!tab) {
    this->RenderError(rc.d2d(), _("No Tab"), rect);
    co_return;
  }
  const auto pageCount = tab->GetPageCount();
  if (pageCount == 0) {
    this->RenderError(rc.d2d(), _("No Pages"), rect);
    co_return;
  }

  co_await tab->RenderPage(rc, tabView->GetPageID(), rect);
}

void TabViewUILayer::RenderError(
  ID2D1DeviceContext* d2d,
  std::string_view text,
  const PixelRect& rect) {
  d2d->FillRectangle(rect, mErrorBackgroundBrush.get());
  mErrorRenderer->Render(d2d, text, rect);
}

std::optional<D2D1_POINT_2F> TabViewUILayer::GetCursorPoint() const {
  return mCursorPoint;
}

}// namespace OpenKneeboard
