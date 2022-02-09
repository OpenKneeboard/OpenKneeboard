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

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>

#include <limits>

#include "CachedLayer.h"
#include "TabWithCursorEvents.h"

namespace OpenKneeboard {

class NavigationTab final : public TabWithCursorEvents {
 public:
  struct Entry {
    std::wstring mName;
    uint16_t mPageIndex;
  };

  NavigationTab(
    const DXResources&,
    Tab* rootTab,
    const std::vector<Entry>& entries,
    const D2D1_SIZE_U& preferredSize);
  ~NavigationTab();

  virtual std::wstring GetTitle() const override;
  virtual void Reload() override;

  virtual uint16_t GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) override;
  virtual void RenderPage(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override;

  virtual void PostCursorEvent(const CursorEvent&, uint16_t pageIndex) override;

 private:
  DXResources mDXR;
  Tab* mRootTab;
  D2D1_SIZE_U mPreferredSize;
  CachedLayer mPreviewLayer;

  struct EntryImpl {
    std::wstring mName;
    uint16_t mPageIndex;
    D2D1_RECT_F mRect;
  };
  std::vector<std::vector<EntryImpl>> mEntries;

  enum class ButtonState {
    NOT_PRESSED,
    PRESSING_BUTTON,
    PRESSING_INACTIVE_AREA,
  };
  ButtonState mButtonState = ButtonState::NOT_PRESSED;
  uint16_t mActiveButton;

  D2D1_POINT_2F mCursorPoint;

  winrt::com_ptr<IDWriteTextFormat> mTextFormat;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;

  uint16_t mPreviewMetricsPage = ~0ui16;
  struct {
    float mBleed;
    float mStroke;
    float mHeight;
    std::vector<D2D1_RECT_F> mRects;
  } mPreviewMetrics;

  void RenderPreviewLayer(
    uint16_t pageIndex,
    ID2D1DeviceContext* ctx,
    const D2D1_SIZE_U& size);

  static constexpr auto PaddingRatio = 1.5f;
};

}// namespace OpenKneeboard
