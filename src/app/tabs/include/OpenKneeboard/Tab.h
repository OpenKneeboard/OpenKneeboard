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

#include <OpenKneeboard/DXResources.h>
#include <d2d1.h>
#include <shims/winrt.h>

#include <memory>
#include <string>

#include "Events.h"
#include "okConfigurableComponent.h"

class wxString;

namespace OpenKneeboard {
struct GameEvent;
struct CursorEvent;
struct DXResources;

class Tab : public okConfigurableComponent {
 public:
  Tab() = delete;
  Tab(const DXResources&, const wxString& title);
  virtual ~Tab();

  std::string GetTitle() const;

  virtual void Reload();

  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;

  virtual uint16_t GetPageCount() const = 0;
  virtual D2D1_SIZE_U GetPreferredPixelSize(uint16_t pageIndex) = 0;
  void RenderPage(uint16_t pageIndex, const D2D1_RECT_F& rect);

  virtual void PostGameEvent(const GameEvent&);
  virtual void PostCursorEvent(const CursorEvent&, uint16_t pageIndex);

  virtual std::shared_ptr<Tab> GetNavigationTab(const D2D1_SIZE_U&);

  Event<> evNeedsRepaintEvent;
  Event<> evFullyReplacedEvent;
  Event<> evPageAppendedEvent;

 protected:
  void ClearDrawings();

  virtual void RenderPageContent(uint16_t pageIndex, const D2D1_RECT_F& rect)
    = 0;

 private:
  DXResources mDXR;
  std::string mTitle;

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
