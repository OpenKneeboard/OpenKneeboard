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

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabState.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <microsoft.ui.xaml.media.dxinterop.h>

#include "Globals.h"

using namespace ::OpenKneeboard;

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
  AddEventListener(gKneeboard->evCursorEvent, [this](const auto& ev) {
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

void TabPage::OnNavigatedTo(const NavigationEventArgs& args) {
  const auto id = winrt::unbox_value<uint64_t>(args.Parameter());
  for (auto tab: gKneeboard->GetTabs()) {
    if (tab->GetInstanceID() != id) {
      continue;
    }
    SetTab(tab);
    break;
  }
}

void TabPage::SetTab(const std::shared_ptr<TabState>& state) {
  mState = state;
  AddEventListener(state->evNeedsRepaintEvent, &TabPage::PaintLater, this);
  AddEventListener(state->evPageChangedEvent, &TabPage::UpdateButtons, this);
  this->UpdateButtons();
}

void TabPage::UpdateButtons() {
  ContentsButton().IsEnabled(mState->SupportsTabMode(TabMode::NAVIGATION));
  ContentsButton().IsChecked(mState->GetTabMode() == TabMode::NAVIGATION);

  const auto index = mState->GetPageIndex();  
  FirstPageButton().IsEnabled(index > 0);
  PreviousPageButton().IsEnabled(index > 0);
  NextPageButton().IsEnabled(index + 1 < mState->GetPageCount());
}

void TabPage::OnCanvasSizeChanged(
  const IInspectable&,
  const SizeChangedEventArgs& args) {
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
  auto metrics = GetPageMetrics();

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
  auto tab = mState->GetTab().get();
  if (tab->GetPageCount()) {
    tab->RenderPage(ctx, mState->GetPageIndex(), metrics.mRenderRect);
  } else {
    mErrorRenderer->Render(ctx, _("No Pages"), metrics.mRenderRect);
  }

  if (!mDrawCursor) {
    return;
  }
  const auto cursorRadius = metrics.mRenderSize.height / CursorRadiusDivisor;
  const auto cursorStroke = metrics.mRenderSize.height / CursorStrokeDivisor;
  ctx->SetTransform(D2D1::Matrix3x2F::Identity());
  auto point = gKneeboard->GetCursorPoint();
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
  const auto contentNativeSize = mState->GetPageCount() == 0 ?
    D2D1_SIZE_U {
      static_cast<UINT>(mCanvasSize.width),
      static_cast<UINT>(mCanvasSize.height),
    }
  : mState->GetNativeContentSize();

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
  const PointerEventArgs& args) {
  for (auto pp: args.GetIntermediatePoints()) {
    this->QueuePointerPoint(pp);
  }
  auto pp = args.CurrentPoint();
  this->QueuePointerPoint(pp);
}

void TabPage::QueuePointerPoint(const PointerPoint& pp) {
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

  gKneeboard->evCursorEvent.Emit({
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

void TabPage::OnFirstPageButtonClicked(const IInspectable&, const RoutedEventArgs&) {
  mState->SetPageIndex(0);
}

void TabPage::OnPreviousPageButtonClicked(const IInspectable&, const RoutedEventArgs&) {
  mState->PreviousPage();
}

void TabPage::OnNextPageButtonClicked(const IInspectable&, const RoutedEventArgs&) {
  mState->NextPage();
}

}// namespace winrt::OpenKneeboardApp::implementation
