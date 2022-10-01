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
#pragma once

#include <OpenKneeboard/CachedLayer.h>
#include <OpenKneeboard/CursorClickableRegions.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/IPageSourceWithNavigation.h>

#include <limits>
#include <memory>

#include "TabBase.h"

namespace OpenKneeboard {

class NavigationTab final : public TabBase,
                            public IPageSourceWithCursorEvents,
                            private EventReceiver {
 public:
  using Entry = NavigationEntry;

  NavigationTab(
    const DXResources&,
    const std::shared_ptr<ITab>& rootTab,
    const std::vector<NavigationEntry>& entries,
    const D2D1_SIZE_U& preferredSize);
  ~NavigationTab();

  virtual utf8_string GetGlyph() const override;
  virtual utf8_string GetTitle() const override;
  virtual void Reload() override;

  virtual PageIndex GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageIndex pageIndex) override;
  virtual void RenderPage(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect) override;

  virtual void PostCursorEvent(
    EventContext,
    const CursorEvent&,
    PageIndex pageIndex) override;

 private:
  DXResources mDXR;
  std::shared_ptr<ITab> mRootTab;
  D2D1_SIZE_U mPreferredSize;
  CachedLayer mPreviewLayer;

  uint16_t mRenderColumns;

  struct Button final {
    winrt::hstring mName;
    PageIndex mPageIndex;
    D2D1_RECT_F mRect;
    uint16_t mRenderColumn;

    bool operator==(const Button&) const noexcept;
  };
  using ButtonTracker = CursorClickableRegions<Button>;
  std::vector<std::shared_ptr<ButtonTracker>> mButtonTrackers;

  winrt::com_ptr<IDWriteTextFormat> mTextFormat;
  winrt::com_ptr<IDWriteTextFormat> mPageNumberTextFormat;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mInactiveBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mPreviewOutlineBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;

  PageIndex mPreviewMetricsPage = ~PageIndex {0};
  struct {
    float mBleed;
    float mStroke;
    float mHeight;
    std::vector<D2D1_RECT_F> mRects;
  } mPreviewMetrics;

  void RenderPreviewLayer(
    PageIndex pageIndex,
    ID2D1DeviceContext* ctx,
    const D2D1_SIZE_U& size);

  static constexpr auto PaddingRatio = 1.5f;
};

}// namespace OpenKneeboard
