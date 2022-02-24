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
#include <d2d1.h>
#include <dxgi1_2.h>

#include <vector>
#include <memory>

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/Events.h>

#include "TabPage.g.h"

namespace OpenKneeboard {
class TabState;
}

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {
struct TabPage : TabPageT<TabPage>, EventReceiver {
  TabPage();
  ~TabPage();

  void OnNavigatedTo(const NavigationEventArgs&);
  void OnSizeChanged(const IInspectable&, const SizeChangedEventArgs&);
  void OnPointerEvent(const IInspectable&, const PointerRoutedEventArgs&);

 private:
  std::shared_ptr<TabState> mState;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  D2D1_COLOR_F mBackgroundColor;

  winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer mCursorTimer { DispatcherQueue().CreateTimer() };
  std::vector<CursorEvent> mCursorEvents;
  void QueuePointerPoint(const winrt::Microsoft::UI::Input::PointerPoint&);
  void FlushCursorEvents();

  void SetTab(const std::shared_ptr<TabState>&);
  void InitializeSwapChain();
  void ResizeSwapChain();
  void PaintNow();

  D2D1_SIZE_F mCanvasSize;

  struct PageMetrics {
    D2D1_SIZE_U mNativeSize;
    D2D1_RECT_F mRenderRect;
    D2D1_SIZE_F mRenderSize;
    float mScale;
  };
  PageMetrics GetPageMetrics();
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabPage : TabPageT<TabPage, implementation::TabPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
