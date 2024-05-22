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

#include "TabBase.h"

#include <OpenKneeboard/CachedLayer.h>
#include <OpenKneeboard/CursorClickableRegions.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/IPageSourceWithNavigation.h>

#include <OpenKneeboard/audited_ptr.h>

#include <limits>
#include <memory>

namespace OpenKneeboard {

class NavigationTab final : public TabBase,
                            public IPageSourceWithCursorEvents,
                            public virtual EventReceiver {
 public:
  using Entry = NavigationEntry;

  NavigationTab() = delete;
  NavigationTab(
    const audited_ptr<DXResources>&,
    const std::shared_ptr<ITab>& rootTab,
    const std::vector<NavigationEntry>& entries);
  ~NavigationTab();

  virtual std::string GetGlyph() const override;
  virtual void Reload() override;

  virtual PageIndex GetPageCount() const override;
  virtual std::vector<PageID> GetPageIDs() const override;
  virtual PreferredSize GetPreferredSize(PageID) override;
  virtual void RenderPage(RenderTarget*, PageID, const PixelRect& rect)
    override;

  virtual void PostCursorEvent(EventContext, const CursorEvent&, PageID)
    override;
  virtual bool CanClearUserInput(PageID) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

 private:
  audited_ptr<DXResources> mDXR;
  std::shared_ptr<ITab> mRootTab;
  PixelSize mPreferredSize;
  std::unordered_map<RenderTargetID, std::unique_ptr<CachedLayer>>
    mPreviewCache;

  uint16_t mRenderColumns;

  struct Button final {
    winrt::hstring mName;
    PageID mPageID;
    D2D1_RECT_F mRect;
    uint16_t mRenderColumn;

    bool operator==(const Button&) const noexcept;
  };
  using ButtonTracker = CursorClickableRegions<Button>;
  std::vector<PageID> mPageIDs;
  std::unordered_map<PageID, std::shared_ptr<ButtonTracker>> mButtonTrackers;
  struct PreviewMetrics {
    float mBleed;
    float mStroke;
    std::vector<PixelRect> mRects;
  };
  std::unordered_map<PageID, PreviewMetrics> mPreviewMetrics;

  winrt::com_ptr<IDWriteTextFormat> mTextFormat;
  winrt::com_ptr<IDWriteTextFormat> mPageNumberTextFormat;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mInactiveBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mPreviewOutlineBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;

  void CalculatePreviewMetrics(PageID);
  // PageID is first for `std::bind_front()`
  void RenderPreviewLayer(PageID, RenderTarget*, const PixelSize& size);

  static constexpr auto PaddingRatio = 1.5f;
};

}// namespace OpenKneeboard
