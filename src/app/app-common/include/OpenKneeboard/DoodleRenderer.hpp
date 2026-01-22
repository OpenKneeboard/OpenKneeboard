// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
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
