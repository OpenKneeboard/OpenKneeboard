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
#include "TabPage.g.h"

#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/Pixels.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/ThreadGuard.hpp>

#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include <d2d1.h>
#include <dxgi1_2.h>

namespace OpenKneeboard {
class CursorRenderer;
class D2DErrorRenderer;
struct DXResources;
class KneeboardView;
class ISelectableToolbarItem;
class IToolbarFlyout;
class IToolbarItem;
class KneeboardState;
class RenderTarget;
class ToolbarAction;
class ToolbarToggleAction;
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
  using TabView = ::OpenKneeboard::TabView;

  TabPage();
  ~TabPage();

  static winrt::fire_and_forget final_release(std::unique_ptr<TabPage>);

  void OnNavigatedTo(const NavigationEventArgs&) noexcept;
  void OnCanvasSizeChanged(
    const IInspectable&,
    const SizeChangedEventArgs&) noexcept;
  void OnPointerEvent(const IInspectable&, const PointerEventArgs&) noexcept;

  void PaintIfDirty();

 private:
  bool mShuttingDown = false;
  winrt::apartment_context mUIThread;
  std::shared_ptr<TabView> mTabView;
  std::unique_ptr<CursorRenderer> mCursorRenderer;
  std::unique_ptr<D2DErrorRenderer> mErrorRenderer;
  D2D1_COLOR_F mBackgroundColor;
  winrt::com_ptr<ID2D1SolidColorBrush> mForegroundBrush;

  DispatcherQueueController mDQC {nullptr};
  InputPointerSource mInputPointerSource {nullptr};
  bool mDrawCursor = false;
  void InitializePointerSource();

  std::mutex mCursorEventsMutex;
  std::queue<CursorEvent> mCursorEvents;
  bool mHaveCursorEvents {false};
  void EnqueuePointerPoint(const PointerPoint&);
  void EnqueueCursorEvent(const CursorEvent&);
  void FlushCursorEvents();

  void SetTab(const std::shared_ptr<TabView>&);
  winrt::fire_and_forget UpdateToolbar();
  void InitializeSwapChain();
  void ResizeSwapChain();

  bool mNeedsFrame = true;
  void PaintLater();
  void PaintNow(
    const std::source_location& loc = std::source_location::current()) noexcept;

  winrt::fire_and_forget OnToolbarActionClick(std::shared_ptr<ToolbarAction>);

  float mCompositionScaleX {1.0f};
  float mCompositionScaleY {1.0f};
  PixelSize mPanelDimensions {};

  struct PageMetrics {
    PixelSize mNativeSize;
    PixelRect mRenderRect;
    PixelSize mRenderSize;
  };
  PageMetrics GetPageMetrics();

  std::vector<std::shared_ptr<IToolbarItem>> mToolbarItems;

  std::vector<EventHandlerToken> mKneeboardViewEvents;
  std::shared_ptr<KneeboardView> mKneeboardView;
  void UpdateKneeboardView();

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

  void AttachVisibility(const std::shared_ptr<IToolbarItem>&, IInspectable);

  winrt::com_ptr<ID3D11Texture2D> mCanvas;
  PixelSize mSwapChainDimensions {};
  winrt::com_ptr<IDXGISwapChain1> mSwapChain;
  std::shared_ptr<RenderTarget> mRenderTarget;

  static std::unordered_map<TabView::RuntimeID, std::weak_ptr<RenderTarget>>
    sRenderTarget;
  static std::shared_ptr<RenderTarget> GetRenderTarget(
    const audited_ptr<DXResources>&,
    const TabView::RuntimeID&);

  audited_ptr<OpenKneeboard::DXResources> mDXR;
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;

  ThreadGuard mThreadGuard;
};
}// namespace winrt::OpenKneeboardApp::implementation
namespace winrt::OpenKneeboardApp::factory_implementation {
struct TabPage : TabPageT<TabPage, implementation::TabPage> {};
}// namespace winrt::OpenKneeboardApp::factory_implementation
