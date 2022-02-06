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
#include <d2d1_2.h>
#include <dxgi1_2.h>
#include <shims/winrt.h>
#include <shims/wx.h>
#include <wx/image.h>

#include <memory>

#include "Events.h"

namespace OpenKneeboard {
struct CursorEvent;
class D2DErrorRenderer;
class KneeboardState;
class TabState;
}// namespace OpenKneeboard

class okTabCanvas final : public wxPanel, private OpenKneeboard::EventReceiver {
 public:
  okTabCanvas(
    wxWindow* parent,
    const OpenKneeboard::DXResources&,
    const std::shared_ptr<OpenKneeboard::KneeboardState>&,
    const std::shared_ptr<OpenKneeboard::TabState>&);
  virtual ~okTabCanvas();

 private:
  void OnSize(wxSizeEvent& ev);
  void OnPaint(wxPaintEvent& ev);
  void OnEraseBackground(wxEraseEvent& ev);

  OpenKneeboard::DXResources mDXR;
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboardState;
  std::shared_ptr<OpenKneeboard::TabState> mTabState;

  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  winrt::com_ptr<ID2D1SolidColorBrush> mCursorBrush;
  std::unique_ptr<OpenKneeboard::D2DErrorRenderer> mErrorRenderer;

  struct PageMetrics {
    D2D1_SIZE_U mNativeSize;
    D2D1_RECT_F mRenderRect;
    D2D1_SIZE_F mRenderSize;
    float mScale;
  };
  PageMetrics GetPageMetrics() const;

  void OnMouseMove(wxMouseEvent&);
  void OnMouseLeave(wxMouseEvent&);

  void OnPixelsChanged(wxCommandEvent&);
};
