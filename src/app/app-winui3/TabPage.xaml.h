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
#include <OpenKneeboard/Events.h>
#include <d2d1.h>
#include <dxgi1_2.h>

#include <memory>
#include <mutex>
#include <vector>

#include "TabPage.g.h"

namespace OpenKneeboard {
class CursorRenderer;
class D2DErrorRenderer;
class ITabView;
class ISelectableToolbarItem;
class IToolbarFlyout;
class IToolbarItem;
class ToolbarAction;
class ToolbarToggleAction;
class IKneeboardView;
}// namespace OpenKneeboard

using namespace winrt::Microsoft::UI::Dispatching;
using namespace winrt::Microsoft::UI::Input;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Navigation;
using namespace OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::OpenKneeboardApp::implementation {
struct TabPage : TabPageT<TabPage>, EventReceiver {
  using ITabView = ::OpenKneeboard::ITabView;

  TabPage();
  ~TabPage();

  static winrt::fire_and_forget final_release(std::unique_ptr<TabPage>);

  void OnNavigatedTo(const NavigationEventArgs&) noexcept;
  void OnCanvasSizeChanged(
    const IInspectable&,
    const SizeChangedEventArgs&) noexcept;
  void OnPointerEvent(const IInspectable&, const PointerEventArgs&) noexcept;

  winrt::Windows::Foundation::IAsyncAction ReleaseDXResources();

 private:
  bool mShuttingDown = false;
  winrt::apartment_context mUIThread;
  std::shared_ptr<IKneeboardView> mKneeboardView;
  std::shared_ptr<ITabView> mTabView;
  std::unique_ptr<CursorRenderer> mCursorRenderer;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  D2D1_COLOR_F mBackgroundColor;
  winrt::com_ptr<ID2D1SolidColorBrush> mForegroundBrush;

  DispatcherQueueController mDQC {nullptr};
  InputPointerSource mInputPointerSource {nullptr};
  bool mDrawCursor = false;
  void InitializePointerSource();
  void QueuePointerPoint(const PointerPoint&);

  void SetTab(const std::shared_ptr<ITabView>&);
  winrt::fire_and_forget UpdateToolbar();
  void InitializeSwapChain();
  void ResizeSwapChain();

  bool mNeedsFrame = true;
  void PaintLater();
  void PaintNow() noexcept;

  winrt::fire_and_forget OnToolbarActionClick(std::shared_ptr<ToolbarAction>);

  D2D1_SIZE_F mCanvasSize;

  struct PageMetrics {
    D2D1_SIZE_U mNativeSize;
    D2D1_RECT_F mRenderRect;
    D2D1_SIZE_F mRenderSize;
  };
  PageMetrics GetPageMetrics();

  std::vector<std::shared_ptr<IToolbarItem>> mToolbarItems;

  muxc::ICommandBarElement CreateCommandBarElement(
    const std::shared_ptr<IToolbarItem>&);
  muxc::AppBarButton CreateAppBarButtonBase(
    const std::shared_ptr<ISelectableToolbarItem>&);
  muxc::AppBarButton CreateAppBarButton(const std::shared_ptr<ToolbarAction>&);
  muxc::AppBarButton CreateAppBarFlyout(const std::shared_ptr<IToolbarFlyout>&);
  muxc::AppBarToggleButton CreateAppBarToggleButton(
    const std::shared_ptr<ToolbarToggleAction>&);

  muxc::MenuFlyoutItemBase CreateMenuFlyoutItem(
    const std::shared_ptr<IToolbarItem>&);
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabPage : TabPageT<TabPage, implementation::TabPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
