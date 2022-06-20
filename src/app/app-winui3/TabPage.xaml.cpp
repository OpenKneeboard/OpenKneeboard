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
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabAction.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <ranges>

#include "Globals.h"

using namespace ::OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

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

  winrt::check_hresult(gDXResources.mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), mCursorBrush.put()));
  mErrorRenderer
    = std::make_unique<D2DErrorRenderer>(gDXResources.mD2DDeviceContext.get());

  this->InitializePointerSource();
  AddEventListener(gKneeboard->evFrameTimerEvent, [this]() {
    if (mNeedsFrame) {
      PaintNow();
    }
  });
  AddEventListener(
    gKneeboard->GetPrimaryViewForDisplay()->evCursorEvent,
    [this](const auto& ev) {
      if (ev.mSource == CursorSource::WINDOW_POINTER) {
        mDrawCursor = false;
      } else {
        mDrawCursor = ev.mTouchState != CursorTouchState::NOT_NEAR_SURFACE;
        PaintLater();
      }
    });
}

TabPage::~TabPage() {
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
  });
}

void TabPage::OnNavigatedTo(const NavigationEventArgs& args) noexcept {
  const auto id = Tab::RuntimeID::FromTemporaryValue(
    winrt::unbox_value<uint64_t>(args.Parameter()));
  mKneeboardView = gKneeboard->GetPrimaryViewForDisplay();
  this->SetTab(mKneeboardView->GetTabViewByID(id));
}

void TabPage::SetTab(const std::shared_ptr<ITabView>& state) {
  mTabView = state;
  AddEventListener(state->evNeedsRepaintEvent, &TabPage::PaintLater, this);

  auto actions = CreateTabActions(state);

  auto commands = CommandBar().PrimaryCommands();
  for (const auto& action: actions) {
    auto toggle = std::dynamic_pointer_cast<TabToggleAction>(action);
    muxc::FontIcon icon;
    icon.Glyph(winrt::to_hstring(action->GetGlyph()));

    if (toggle) {
      muxc::AppBarToggleButton button;
      button.Label(winrt::to_hstring(action->GetLabel()));
      button.Icon(icon);
      button.IsEnabled(action->IsEnabled());

      button.IsChecked(toggle->IsActive());
      button.Checked([toggle](auto, auto) { toggle->Activate(); });
      button.Unchecked([toggle](auto, auto) { toggle->Deactivate(); });

      AddEventListener(
        toggle->evStateChangedEvent,
        [this, toggle, button]() noexcept -> winrt::fire_and_forget {
          co_await mUIThread;
          button.IsChecked(toggle->IsActive());
          button.IsEnabled(toggle->IsEnabled());
        });

      commands.Append(button);
      continue;
    }

    muxc::AppBarButton button;
    button.Label(winrt::to_hstring(action->GetLabel()));
    button.Icon(icon);
    button.IsEnabled(action->IsEnabled());

    button.Click([action](auto, auto) { action->Execute(); });

    AddEventListener(
      action->evStateChangedEvent,
      [this, action, button]() noexcept -> winrt::fire_and_forget {
        co_await mUIThread;
        button.IsEnabled(action->IsEnabled());
      });

    commands.Append(button);
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

void TabPage::PaintNow() {
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
  const auto cursorRadius = metrics.mRenderSize.height / CursorRadiusDivisor;
  const auto cursorStroke = metrics.mRenderSize.height / CursorStrokeDivisor;
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  auto point = mKneeboardView->GetCursorPoint();
  point.x *= metrics.mScale;
  point.y *= metrics.mScale;
  point.x += metrics.mRenderRect.left;
  point.y += metrics.mRenderRect.top;
  ctx->DrawEllipse(
    D2D1::Ellipse(point, cursorRadius, cursorRadius),
    mCursorBrush.get(),
    cursorStroke);
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

  auto positionState
    = (x >= metrics.mRenderRect.left && x <= metrics.mRenderRect.right
       && y >= metrics.mRenderRect.top && y <= metrics.mRenderRect.bottom)
    ? CursorPositionState::IN_CONTENT_RECT
    : CursorPositionState::NOT_IN_CONTENT_RECT;

  x -= metrics.mRenderRect.left;
  y -= metrics.mRenderRect.top;
  x /= metrics.mScale;
  y /= metrics.mScale;

  auto ppp = pp.Properties();

  const bool leftClick = ppp.IsLeftButtonPressed();
  const bool rightClick = ppp.IsRightButtonPressed();

  mKneeboardView->PostCursorEvent({
    .mSource = CursorSource::WINDOW_POINTER,
    .mPositionState = positionState,
    .mTouchState = (leftClick || rightClick)
      ? CursorTouchState::TOUCHING_SURFACE
      : CursorTouchState::NEAR_SURFACE,
    .mX = x,
    .mY = y,
    .mPressure = rightClick ? 0.8f : 0.0f,
    .mButtons = rightClick ? (1ui32 << 1) : 1ui32,
  });
}

}// namespace winrt::OpenKneeboardApp::implementation
