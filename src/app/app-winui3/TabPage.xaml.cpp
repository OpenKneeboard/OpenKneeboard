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
// clang-format off
#include "pch.h"
#include "TabPage.xaml.h"
#include "TabPage.g.cpp"
// clang-format on

#include <OpenKneeboard/CreateTabActions.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/CursorRenderer.h>
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabAction.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <ranges>

#include "Globals.h"

using namespace ::OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxcp = winrt::Microsoft::UI::Xaml::Controls::Primitives;

namespace winrt::OpenKneeboardApp::implementation {

TabPage::TabPage() {
  InitializeComponent();

  auto brush
    = Background().as<::winrt::Microsoft::UI::Xaml::Media::SolidColorBrush>();
  auto color = brush.Color();
  mBackgroundColor = {
    color.R / 255.0f,
    color.G / 255.0f,
    color.B / 255.0f,
    color.A / 255.0f,
  };
  brush = {nullptr};
  brush
    = Foreground().as<::winrt::Microsoft::UI::Xaml::Media::SolidColorBrush>();
  color = brush.Color();

  winrt::check_hresult(gDXResources.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(
      color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f),
    mForegroundBrush.put()));

  mCursorRenderer = std::make_unique<CursorRenderer>(gDXResources);
  mErrorRenderer
    = std::make_unique<D2DErrorRenderer>(gDXResources.mD2DDeviceContext.get());

  this->InitializePointerSource();
  AddEventListener(gKneeboard->evFrameTimerEvent, [this]() {
    if (mNeedsFrame) {
      PaintNow();
    }
  });
}

TabPage::~TabPage() {
  this->RemoveAllEventListeners();
}

winrt::fire_and_forget TabPage::final_release(
  std::unique_ptr<TabPage> instance) {
  co_await instance->mDQC.ShutdownQueueAsync();
  instance->RemoveAllEventListeners();
}

void TabPage::InitializePointerSource() {
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
  mDQC.DispatcherQueue().TryEnqueue([this]() {
    mInputPointerSource = this->Canvas().CreateCoreIndependentInputSource(
      InputPointerSourceDeviceKinds::Mouse | InputPointerSourceDeviceKinds::Pen
      | InputPointerSourceDeviceKinds::Touch);
    mInputPointerSource.PointerMoved({this, &TabPage::OnPointerEvent});
    mInputPointerSource.PointerPressed({this, &TabPage::OnPointerEvent});
    mInputPointerSource.PointerReleased({this, &TabPage::OnPointerEvent});
    mInputPointerSource.PointerExited([this](const auto&, const auto&) {
      mKneeboardView->PostCursorEvent({});
    });
  });
}

void TabPage::OnNavigatedTo(const NavigationEventArgs& args) noexcept {
  const auto id = ITab::RuntimeID::FromTemporaryValue(
    winrt::unbox_value<uint64_t>(args.Parameter()));
  mKneeboardView = gKneeboard->GetActiveViewForGlobalInput();
  AddEventListener(mKneeboardView->evCursorEvent, [this](const auto& ev) {
    if (ev.mSource == CursorSource::WINDOW_POINTER) {
      mDrawCursor = false;
    } else {
      mDrawCursor = ev.mTouchState != CursorTouchState::NOT_NEAR_SURFACE;
      PaintLater();
    }
  });

  this->SetTab(mKneeboardView->GetTabViewByID(id));
}

muxc::ICommandBarElement TabPage::CreateCommandBarElement(
  const std::shared_ptr<TabAction>& action) {
  auto toggle = std::dynamic_pointer_cast<TabToggleAction>(action);
  if (toggle) {
    return CreateAppBarToggleButton(toggle);
  }

  return CreateAppBarButton(action);
}

muxc::AppBarToggleButton TabPage::CreateAppBarToggleButton(
  const std::shared_ptr<TabToggleAction>& action) {
  muxc::AppBarToggleButton button;

  muxc::FontIcon icon;
  icon.Glyph(winrt::to_hstring(action->GetGlyph()));
  button.Icon(icon);

  button.Label(winrt::to_hstring(action->GetLabel()));
  button.IsEnabled(action->IsEnabled());

  button.IsChecked(action->IsActive());
  button.Checked([action](auto, auto) { action->Activate(); });
  button.Unchecked([action](auto, auto) { action->Deactivate(); });

  AddEventListener(
    action->evStateChangedEvent,
    [this, action, button]() noexcept -> winrt::fire_and_forget {
      auto [action, button] = std::make_tuple(action, button);
      co_await mUIThread;
      button.IsChecked(action->IsActive());
      button.IsEnabled(action->IsEnabled());
    });

  return button;
}

muxc::AppBarButton TabPage::CreateAppBarButton(
  const std::shared_ptr<TabAction>& action) {
  muxc::AppBarButton button;

  muxc::FontIcon icon;
  icon.Glyph(winrt::to_hstring(action->GetGlyph()));
  button.Icon(icon);

  button.Label(winrt::to_hstring(action->GetLabel()));
  button.IsEnabled(action->IsEnabled());

  button.Click([action](auto, auto) { action->Execute(); });

  AddEventListener(
    action->evStateChangedEvent,
    [this, action, button]() noexcept -> winrt::fire_and_forget {
      auto [action, button] = std::make_tuple(action, button);
      co_await mUIThread;
      button.IsEnabled(action->IsEnabled());
    });
  return button;
}

void TabPage::SetTab(const std::shared_ptr<ITabView>& state) {
  mTabView = state;
  AddEventListener(state->evNeedsRepaintEvent, &TabPage::PaintLater, this);

  auto actions = InAppActions::Create(gKneeboard.get(), mKneeboardView, state);

  auto primary = CommandBar().PrimaryCommands();
  for (const auto& action: actions.mPrimary) {
    primary.Append(CreateCommandBarElement(action));
  }

  auto secondary = CommandBar().SecondaryCommands();
  for (const auto& action: actions.mSecondary) {
    secondary.Append(CreateCommandBarElement(action));
  }
}

void TabPage::OnCanvasSizeChanged(
  const IInspectable&,
  const SizeChangedEventArgs& args) noexcept {
  auto size = args.NewSize();
  if (size.Width < 1 || size.Height < 1) {
    mSwapChain = nullptr;
    return;
  }
  mCanvasSize = {
    static_cast<FLOAT>(size.Width),
    static_cast<FLOAT>(size.Height),
  };
  std::scoped_lock lock(mSwapChainMutex);
  if (mSwapChain) {
    ResizeSwapChain();
  } else {
    InitializeSwapChain();
  }
  PaintLater();
}

void TabPage::ResizeSwapChain() {
  DXGI_SWAP_CHAIN_DESC desc;
  winrt::check_hresult(mSwapChain->GetDesc(&desc));
  winrt::check_hresult(mSwapChain->ResizeBuffers(
    desc.BufferCount,
    static_cast<UINT>(mCanvasSize.width * Canvas().CompositionScaleX()),
    static_cast<UINT>(mCanvasSize.height * Canvas().CompositionScaleY()),
    desc.BufferDesc.Format,
    desc.Flags));
}

void TabPage::InitializeSwapChain() {
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
    .Width
    = static_cast<UINT>(mCanvasSize.width * Canvas().CompositionScaleX()),
    .Height
    = static_cast<UINT>(mCanvasSize.height * Canvas().CompositionScaleY()),
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = 2,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
  };
  const DXResources& dxr = gDXResources;
  winrt::check_hresult(dxr.mDXGIFactory->CreateSwapChainForComposition(
    dxr.mDXGIDevice.get(), &swapChainDesc, nullptr, mSwapChain.put()));
  Canvas().as<ISwapChainPanelNative>()->SetSwapChain(mSwapChain.get());
}

void TabPage::PaintLater() {
  mNeedsFrame = true;
}

void TabPage::PaintNow() noexcept {
  if (!mSwapChain) {
    return;
  }
  auto ctx = gDXResources.mD2DDeviceContext.get();

  std::scoped_lock lock(mSwapChainMutex);
  winrt::com_ptr<IDXGISurface> surface;
  winrt::check_hresult(mSwapChain->GetBuffer(0, IID_PPV_ARGS(surface.put())));
  winrt::com_ptr<ID2D1Bitmap1> bitmap;
  winrt::check_hresult(
    ctx->CreateBitmapFromDxgiSurface(surface.get(), nullptr, bitmap.put()));
  ctx->SetTarget(bitmap.get());
  ctx->BeginDraw();
  const auto cleanup = scope_guard([ctx, this]() {
    ctx->EndDraw();
    ctx->SetTarget(nullptr);
    mSwapChain->Present(0, 0);
    mNeedsFrame = false;
  });
  ctx->Clear(mBackgroundColor);

  if (!mTabView) {
    DXGI_SURFACE_DESC desc;
    surface->GetDesc(&desc);
    mErrorRenderer->Render(
      ctx,
      _("Missing or Deleted Tab"),
      {
        0,
        0,
        static_cast<FLOAT>(desc.Width),
        static_cast<FLOAT>(desc.Height),
      });
    return;
  }

  auto metrics = GetPageMetrics();
  auto tab = mTabView->GetTab();
  if (tab->GetPageCount()) {
    tab->RenderPage(ctx, mTabView->GetPageIndex(), metrics.mRenderRect);
  } else {
    mErrorRenderer->Render(
      ctx, _("No Pages"), metrics.mRenderRect, mForegroundBrush.get());
  }

  if (!mDrawCursor) {
    return;
  }

  auto maybePoint = mKneeboardView->GetCursorContentPoint();
  if (!maybePoint) {
    return;
  }
  auto point = *maybePoint;
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  point.x *= metrics.mScale;
  point.y *= metrics.mScale;
  point.x += metrics.mRenderRect.left;
  point.y += metrics.mRenderRect.top;
  mCursorRenderer->Render(ctx, point, metrics.mRenderSize);
}

TabPage::PageMetrics TabPage::GetPageMetrics() {
  if (!mTabView) {
    throw std::logic_error("Attempt to fetch Page Metrics without a tab");
  }
  const auto contentNativeSize = mTabView->GetPageCount() == 0 ?
    D2D1_SIZE_U {
      static_cast<UINT>(mCanvasSize.width),
      static_cast<UINT>(mCanvasSize.height),
    }
  : mTabView->GetNativeContentSize();

  const auto scaleX = mCanvasSize.width / contentNativeSize.width;
  const auto scaleY = mCanvasSize.height / contentNativeSize.height;
  const auto scale = std::min(scaleX, scaleY);

  const D2D1_SIZE_F contentRenderSize {
    contentNativeSize.width * scale, contentNativeSize.height * scale};
  const auto padX = (mCanvasSize.width - contentRenderSize.width) / 2;
  const auto padY = (mCanvasSize.height - contentRenderSize.height) / 2;

  const D2D1_RECT_F contentRenderRect {
    .left = padX,
    .top = padY,
    .right = mCanvasSize.width - padX,
    .bottom = mCanvasSize.height - padY,
  };

  return {contentNativeSize, contentRenderRect, contentRenderSize, scale};
}

void TabPage::OnPointerEvent(
  const IInspectable&,
  const PointerEventArgs& args) noexcept {
  for (auto pp: args.GetIntermediatePoints()) {
    this->QueuePointerPoint(pp);
  }
  auto pp = args.CurrentPoint();
  this->QueuePointerPoint(pp);
}

void TabPage::QueuePointerPoint(const PointerPoint& pp) {
  if (!mKneeboardView) {
    return;
  }
  if (!mTabView) {
    return;
  }
  const auto metrics = GetPageMetrics();
  auto x = static_cast<FLOAT>(pp.Position().X);
  auto y = static_cast<FLOAT>(pp.Position().Y);

  x -= metrics.mRenderRect.left;
  y -= metrics.mRenderRect.top;
  x /= metrics.mScale;
  y /= metrics.mScale;

  auto ppp = pp.Properties();

  const bool leftClick = ppp.IsLeftButtonPressed();
  const bool rightClick = ppp.IsRightButtonPressed();

  const auto canvasPoint = mKneeboardView->GetCursorCanvasPoint({x, y});

  if (
    canvasPoint.x < 0 || canvasPoint.x > 1 || canvasPoint.y < 0
    || canvasPoint.y > 1) {
    mKneeboardView->PostCursorEvent({});
    return;
  }

  uint32_t buttons = 0;
  if (leftClick) {
    buttons |= 1;
  }
  if (rightClick) {
    buttons |= (1 << 1);
  }

  mKneeboardView->PostCursorEvent({
    .mSource = CursorSource::WINDOW_POINTER,
    .mTouchState = (leftClick || rightClick)
      ? CursorTouchState::TOUCHING_SURFACE
      : CursorTouchState::NEAR_SURFACE,
    .mX = canvasPoint.x,
    .mY = canvasPoint.y,
    .mPressure = rightClick ? 0.8f : 0.0f,
    .mButtons = buttons,
  });
}

}// namespace winrt::OpenKneeboardApp::implementation
