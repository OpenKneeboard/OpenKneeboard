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
#include <d2d1.h>
#include <shims/winrt.h>

#include <memory>

#include <OpenKneeboard/CachedLayer.h>
#include "TabWithCursorEvents.h"

namespace OpenKneeboard {

class TabWithDoodles : public virtual TabWithCursorEvents,
                       private EventReceiver {
 public:
  TabWithDoodles(const DXResources&);
  virtual ~TabWithDoodles();

  virtual void RenderPage(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override final;
  virtual void PostCursorEvent(const CursorEvent&, uint16_t pageIndex) override;

 protected:
  virtual void RenderPageContent(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F&)
    = 0;
  virtual void RenderOverDoodles(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F&);

  void ClearContentCache();
  void ClearDoodles();

 private:
  DXResources mDXR;

  CachedLayer mContentLayer;
  winrt::com_ptr<ID2D1SolidColorBrush> mBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mEraser;

  struct Drawing {
    winrt::com_ptr<IDXGISurface> mSurface;
    winrt::com_ptr<ID2D1Bitmap1> mBitmap;
    float mScale;
    std::vector<CursorEvent> mBufferedEvents;
    bool mHaveCursor = false;
    D2D1_POINT_2F mCursorPoint;
  };
  winrt::com_ptr<ID2D1DeviceContext> mDrawingContext;
  std::vector<Drawing> mDrawings;

  ID2D1Bitmap* GetDrawingSurface(
    uint16_t index,
    const D2D1_SIZE_U& contentPixels);

  void FlushCursorEvents();
};
}// namespace OpenKneeboard
