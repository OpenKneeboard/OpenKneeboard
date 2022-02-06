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
#include <OpenKneeboard/Tab.h>

namespace OpenKneeboard {

class NavigationTab final : public Tab {
 public:
  struct Entry {
    std::wstring mName;
    uint16_t mPageIndex;
  };

  NavigationTab(
    const DXResources&,
    const D2D1_SIZE_U& preferredSize,
    const std::vector<Entry>& entries);
  ~NavigationTab();

  virtual uint16_t GetPageCount() const override;
  virtual D2D1_SIZE_U GetPreferredPixelSize(uint16_t pageIndex) override;
  virtual void PostCursorEvent(const CursorEvent&, uint16_t pageIndex) override;

 protected:
  virtual void RenderPageContent(uint16_t pageIndex, const D2D1_RECT_F& rect)
    override;

 private:
  DXResources mDXR;
  D2D1_SIZE_U mPreferredSize;

  struct EntryImpl {
    std::wstring mName;
    uint16_t mPageIndex;
    D2D1_RECT_F mRect;
  };
  std::vector<std::vector<EntryImpl>> mEntries;

  D2D1_POINT_2F mCursorPoint;

  winrt::com_ptr<IDWriteTextFormat> mTextFormat;
  winrt::com_ptr<ID2D1SolidColorBrush> mBackgroundBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mHighlightBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mTextBrush;
};

}// namespace OpenKneeboard
