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

#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/RenderTarget.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/inttypes.hpp>

#include <unordered_set>

namespace OpenKneeboard {

class KneeboardState;

class DoodleRenderer final {
 public:
  DoodleRenderer(const audited_ptr<DXResources>&, KneeboardState*);
  ~DoodleRenderer();

  void Render(ID2D1DeviceContext*, PageID, const PixelRect& destRect);
  void Render(RenderTarget*, PageID, const PixelRect& destRect);

  void PostCursorEvent(
    KneeboardViewID,
    const CursorEvent&,
    PageID,
    const PixelSize& nativePageSize);

  bool HaveDoodles() const;
  bool HaveDoodles(PageID) const;
  void Clear();
  void ClearPage(PageID);
  void ClearExcept(const std::unordered_set<PageID>&);

  Event<> evNeedsRepaintEvent;
  Event<> evAddedPageEvent;

 private:
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;

  winrt::com_ptr<ID2D1SolidColorBrush> mBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> mEraser;

  struct Drawing {
    winrt::com_ptr<IDXGISurface> mSurface;
    winrt::com_ptr<ID2D1Bitmap1> mBitmap;
    float mScale {-1.0f};
    std::vector<CursorEvent> mBufferedEvents;
    bool mHaveCursor {false};
    Geometry2D::Point<float> mCursorPoint;
    PixelSize mNativeSize {0, 0};
  };
  winrt::com_ptr<ID2D1DeviceContext> mDrawingContext;
  std::mutex mBufferedEventsMutex;
  std::unordered_map<PageID, Drawing> mDrawings;

  ID2D1Bitmap* GetDrawingSurface(PageID);

  void FlushCursorEvents();

  ThreadGuard mThreadGuard;
};

}// namespace OpenKneeboard
