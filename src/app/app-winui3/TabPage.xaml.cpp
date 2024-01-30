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

#include "Globals.h"

#include <OpenKneeboard/CreateTabActions.h>
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/CursorRenderer.h>
#include <OpenKneeboard/D2DErrorRenderer.h>
#include <OpenKneeboard/ICheckableToolbarItem.h>
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/IToolbarFlyout.h>
#include <OpenKneeboard/IToolbarItemWithConfirmation.h>
#include <OpenKneeboard/IToolbarItemWithVisibility.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/ToolbarSeparator.h>
#include <OpenKneeboard/ToolbarToggleAction.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/weak_wrap.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <microsoft.ui.xaml.media.dxinterop.h>

#include <mutex>
#include <ranges>

using namespace ::OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxcp = winrt::Microsoft::UI::Xaml::Controls::Primitives;

namespace winrt::OpenKneeboardApp::implementation {

TabPage::TabPage() {
  InitializeComponent();
  mDXR = gDXResources.lock();

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

  winrt::check_hresult(mDXR->mD2DDeviceContext->CreateSolidColorBrush(
    D2D1::ColorF(
      color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f),
    mForegroundBrush.put()));

  mCursorRenderer = std::make_unique<CursorRenderer>(mDXR);
  mErrorRenderer = std::make_unique<D2DErrorRenderer>(mDXR);

  this->InitializePointerSource();
  AddEventListener(
    gKneeboard->evFrameTimerEvent, weak_wrap(this)([](auto self) {
      TraceLoggingWrite(
        gTraceProvider,
        "TabPageTickHandler",
        TraceLoggingValue(self->mNeedsFrame, "NeedsFrame"));
      if (self->mNeedsFrame) {
        self->PaintNow();
      }
    }));
  OpenKneeboardApp::TabPage projected {*this};
  gTabs.push_back(winrt::make_weak(projected));
}

TabPage::~TabPage() = default;

winrt::fire_and_forget TabPage::final_release(
  std::unique_ptr<TabPage> instance) {
  instance->RemoveAllEventListeners();

  // Work around https://github.com/microsoft/microsoft-ui-xaml/issues/7254
  auto uiThread = instance->mUIThread;
  auto swapChain = instance->mSwapChain;
  co_await instance->ReleaseDXResources();
  co_await uiThread;
  instance.reset();
  // 0 doesn't work - I think we just need to re-enter the message pump/event
  // loop
  co_await winrt::resume_after(std::chrono::milliseconds(1));
  co_await uiThread;
  swapChain = {nullptr};
}

winrt::Windows::Foundation::IAsyncAction TabPage::ReleaseDXResources() {
  auto keepAlive = this->get_strong();
  if (mShuttingDown) {
    co_return;
  }
  mShuttingDown = true;

  co_await mUIThread;
  this->RemoveAllEventListeners();
  {
    const std::unique_lock lock(*mDXR);
    Canvas().as<ISwapChainPanelNative>()->SetSwapChain(nullptr);
    mKneeboardView = {};
    mTabView = {};
    mCursorRenderer = {};
    mErrorRenderer = {};
    mSwapChain = {};
    mForegroundBrush = {};
    mToolbarItems = {};

    CommandBar().PrimaryCommands().Clear();
    CommandBar().SecondaryCommands().Clear();
  }

  mDXR = {};
}

void TabPage::InitializePointerSource() {
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
  mDQC.DispatcherQueue().TryEnqueue([weak = get_weak()]() {
    auto self = weak.get();
    if (!self) {
      return;
    }

    auto& ips = self->mInputPointerSource;
    ips = self->Canvas().CreateCoreIndependentInputSource(
      InputPointerSourceDeviceKinds::Mouse | InputPointerSourceDeviceKinds::Pen
      | InputPointerSourceDeviceKinds::Touch);

    auto onPointerEvent = [weak](const auto& a, const auto& b) {
      if (auto self = weak.get()) {
        self->OnPointerEvent(a, b);
      }
    };
    ips.PointerMoved(onPointerEvent);
    ips.PointerPressed(onPointerEvent);
    ips.PointerReleased(onPointerEvent);
    ips.PointerExited([weak](const auto&, const auto&) {
      if (auto self = weak.get()) {
        self->mKneeboardView->PostCursorEvent({});
      }
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
  const std::shared_ptr<IToolbarItem>& item) {
  auto sep = std::dynamic_pointer_cast<ToolbarSeparator>(item);
  if (sep) {
    return muxc::AppBarSeparator {};
  }

  auto toggle = std::dynamic_pointer_cast<ToolbarToggleAction>(item);
  if (toggle) {
    return CreateAppBarToggleButton(toggle);
  }

  auto action = std::dynamic_pointer_cast<ToolbarAction>(item);
  if (action) {
    return CreateAppBarButton(action);
  }

  auto flyout = std::dynamic_pointer_cast<IToolbarFlyout>(item);
  if (flyout) {
    return CreateAppBarFlyout(flyout);
  }

  OPENKNEEBOARD_BREAK;
  return {nullptr};
}

muxc::AppBarToggleButton TabPage::CreateAppBarToggleButton(
  const std::shared_ptr<ToolbarToggleAction>& action) {
  muxc::AppBarToggleButton button;

  muxc::FontIcon icon;
  icon.Glyph(winrt::to_hstring(action->GetGlyph()));
  button.Icon(icon);

  button.Label(winrt::to_hstring(action->GetLabel()));
  button.IsEnabled(action->IsEnabled());

  button.IsChecked(action->IsActive());
  button.Checked(discard_winrt_event_args(
    weak_wrap(action)([](auto action) { action->Activate(); })));
  button.Unchecked(discard_winrt_event_args(
    weak_wrap(action)([](auto action) { action->Deactivate(); })));

  AddEventListener(
    action->evStateChangedEvent,
    weak_wrap(this, action, button)(
      [](auto self, auto action, auto button) noexcept
      -> winrt::fire_and_forget {
        co_await self->mUIThread;
        button.IsChecked(action->IsActive());
        button.IsEnabled(action->IsEnabled());
      }));

  return button;
}

muxc::AppBarButton TabPage::CreateAppBarButtonBase(
  const std::shared_ptr<ISelectableToolbarItem>& action) {
  muxc::AppBarButton button;

  if (!action->GetGlyph().empty()) {
    muxc::FontIcon icon;
    icon.Glyph(winrt::to_hstring(action->GetGlyph()));
    button.Icon(icon);
  }

  button.Label(winrt::to_hstring(action->GetLabel()));
  button.IsEnabled(action->IsEnabled());

  AddEventListener(
    action->evStateChangedEvent,
    weak_wrap(this, action, button)(
      [](auto self, auto action, auto button) noexcept
      -> winrt::fire_and_forget {
        co_await self->mUIThread;
        button.IsEnabled(action->IsEnabled());
      }));

  return button;
}

winrt::fire_and_forget TabPage::OnToolbarActionClick(
  std::shared_ptr<ToolbarAction> action) {
  auto confirm
    = std::dynamic_pointer_cast<IToolbarItemWithConfirmation>(action);
  if (!confirm) {
    action->Execute();
    co_return;
  }

  co_await mUIThread;
  muxc::ContentDialog dialog;
  dialog.XamlRoot(this->XamlRoot());
  dialog.Title(box_value(to_hstring(confirm->GetConfirmationTitle())));
  dialog.Content(box_value(to_hstring(confirm->GetConfirmationDescription())));
  dialog.PrimaryButtonText(to_hstring(confirm->GetConfirmButtonLabel()));
  dialog.CloseButtonText(to_hstring(confirm->GetCancelButtonLabel()));
  dialog.DefaultButton(muxc::ContentDialogButton::Primary);

  if (co_await dialog.ShowAsync() != muxc::ContentDialogResult::Primary) {
    co_return;
  }

  action->Execute();
}

muxc::AppBarButton TabPage::CreateAppBarButton(
  const std::shared_ptr<ToolbarAction>& action) {
  auto button = CreateAppBarButtonBase(action);
  button.Click(discard_winrt_event_args(weak_wrap(this, action)(
    [](auto self, auto action) { self->OnToolbarActionClick(action); })));
  return button;
}

muxc::MenuFlyoutItemBase TabPage::CreateMenuFlyoutItem(
  const std::shared_ptr<IToolbarItem>& item) {
  const auto action = std::dynamic_pointer_cast<ToolbarAction>(item);
  if (!action) {
    OPENKNEEBOARD_BREAK;
    return nullptr;
  }

  muxc::MenuFlyoutItem ret {nullptr};
  auto checkable = std::dynamic_pointer_cast<ICheckableToolbarItem>(item);
  if (checkable) {
    auto tmfi = muxc::ToggleMenuFlyoutItem {};
    ret = tmfi;
    tmfi.IsChecked(checkable->IsChecked());
    AddEventListener(
      checkable->evStateChangedEvent,
      weak_wrap(this, tmfi, checkable)(
        [](auto self, auto tmfi, auto checkable) -> winrt::fire_and_forget {
          co_await self->mUIThread;
          tmfi.IsChecked(checkable->IsChecked());
        }));
  } else {
    ret = {};
  }
  ret.Text(winrt::to_hstring(action->GetLabel()));
  ret.IsEnabled(action->IsEnabled());
  ret.Click(discard_winrt_event_args(weak_wrap(this, action)(
    [](auto self, auto action) { self->OnToolbarActionClick(action); })));

  AddEventListener(
    action->evStateChangedEvent,
    weak_wrap(this, action, ret)(
      [](auto self, auto action, auto ret) noexcept -> winrt::fire_and_forget {
        co_await self->mUIThread;
        ret.IsEnabled(action->IsEnabled());
      }));
  return ret;
}

muxc::AppBarButton TabPage::CreateAppBarFlyout(
  const std::shared_ptr<IToolbarFlyout>& item) {
  // TODO (WinUI upgrade): there should be chevrons for these, but aren't
  // even when done in the XAML. Report bug if still present in latest
  auto button = CreateAppBarButtonBase(item);

  muxc::MenuFlyout flyout;
  for (auto& item: item->GetSubItems()) {
    const auto flyoutItem = CreateMenuFlyoutItem(item);
    if (flyoutItem) {
      AttachVisibility(item, flyoutItem);
      flyout.Items().Append(flyoutItem);
    }
  }

  button.Flyout(flyout);

  return button;
}

void TabPage::SetTab(const std::shared_ptr<ITabView>& state) {
  mTabView = state;
  AddEventListener(
    state->evNeedsRepaintEvent, std::bind_front(&TabPage::PaintLater, this));

  this->UpdateToolbar();
}

winrt::fire_and_forget TabPage::UpdateToolbar() {
  co_await mUIThread;
  auto actions
    = InAppActions::Create(gKneeboard.get(), mKneeboardView, mTabView);
  {
    std::vector keepAlive = actions.mPrimary;
    keepAlive.reserve(keepAlive.size() + actions.mSecondary.size());
    std::ranges::copy(actions.mSecondary, std::back_inserter(keepAlive));
    mToolbarItems = keepAlive;
  }

  auto primary = CommandBar().PrimaryCommands();
  primary.Clear();
  for (const auto& item: actions.mPrimary) {
    auto element = CreateCommandBarElement(item);
    if (element) {
      AttachVisibility(item, element);
      primary.Append(element);
    }
  }

  auto secondary = CommandBar().SecondaryCommands();
  secondary.Clear();
  for (const auto& item: actions.mSecondary) {
    auto element = CreateCommandBarElement(item);
    if (element) {
      AttachVisibility(item, element);
      secondary.Append(element);
    }
  }
}

void TabPage::AttachVisibility(
  const std::shared_ptr<IToolbarItem>& item,
  IInspectable inspectable) {
  auto visibility = std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
  if (!visibility) {
    return;
  }
  auto control = inspectable.as<UIElement>();
  control.Visibility(
    visibility->IsVisible() ? Visibility::Visible : Visibility::Collapsed);
  AddEventListener(
    visibility->evStateChangedEvent,
    weak_wrap(this, visibility, control)(
      [](auto self, auto visibility, auto control) -> winrt::fire_and_forget {
        co_await self->mUIThread;
        control.Visibility(
          visibility->IsVisible() ? Visibility::Visible
                                  : Visibility::Collapsed);
      }));
}

void TabPage::OnCanvasSizeChanged(
  const IInspectable&,
  const SizeChangedEventArgs& args) noexcept {
  auto size = args.NewSize();
  if (size.Width < 1 || size.Height < 1) {
    mSwapChain = nullptr;
    return;
  }

  const auto canvas = Canvas();
  mCompositionScaleX = canvas.CompositionScaleX();
  mCompositionScaleY = canvas.CompositionScaleY();
  mCanvasSize = {
    static_cast<FLOAT>(size.Width) * mCompositionScaleX,
    static_cast<FLOAT>(size.Height) * mCompositionScaleY,
  };
  const std::unique_lock lock(*mDXR);
  if (mSwapChain) {
    ResizeSwapChain();
  } else {
    InitializeSwapChain();
  }
  PaintNow();
}

void TabPage::ResizeSwapChain() {
  mCanvas = nullptr;
  mRenderTarget = {};

  DXGI_SWAP_CHAIN_DESC desc;
  winrt::check_hresult(mSwapChain->GetDesc(&desc));
  winrt::check_hresult(mSwapChain->ResizeBuffers(
    desc.BufferCount,
    static_cast<UINT>(mCanvasSize.width),
    static_cast<UINT>(mCanvasSize.height),
    desc.BufferDesc.Format,
    desc.Flags));

  winrt::check_hresult(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mCanvas.put())));
  mRenderTarget = RenderTarget::Create(mDXR, mCanvas);
}

void TabPage::InitializeSwapChain() {
  if (mShuttingDown) {
    return;
  }
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
    .Width = static_cast<UINT>(mCanvasSize.width),
    .Height = static_cast<UINT>(mCanvasSize.height),
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = 2,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
  };
  winrt::check_hresult(mDXR->mDXGIFactory->CreateSwapChainForComposition(
    mDXR->mDXGIDevice.get(), &swapChainDesc, nullptr, mSwapChain.put()));

  winrt::check_hresult(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mCanvas.put())));
  mRenderTarget = RenderTarget::Create(mDXR, mCanvas);
  Canvas().as<ISwapChainPanelNative>()->SetSwapChain(mSwapChain.get());
}

void TabPage::PaintLater() {
  TraceLoggingWrite(gTraceProvider, "TabPage::PaintLater()");
  mNeedsFrame = true;
}

void TabPage::PaintNow() noexcept {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(activity, "TabPage::PaintNow()");
  if (!mSwapChain) {
    TraceLoggingWriteStop(
      activity,
      "TabPage::PaintNow()",
      TraceLoggingValue("No swapchain", "Result"));
    return;
  }
  const std::unique_lock lock(*mDXR);
  const auto cleanup = scope_guard([this]() {
    OPENKNEEBOARD_TraceLoggingScope("TabPage/Present()");
    mSwapChain->Present(0, 0);
    mNeedsFrame = false;
  });

  {
    auto ctx = mRenderTarget->d2d();
    ctx->Clear(mBackgroundColor);

    if (!mTabView) {
      D3D11_TEXTURE2D_DESC desc;
      mCanvas->GetDesc(&desc);
      mErrorRenderer->Render(
        ctx,
        _("Missing or Deleted Tab"),
        {
          0,
          0,
          static_cast<FLOAT>(desc.Width),
          static_cast<FLOAT>(desc.Height),
        });
      TraceLoggingWriteStop(
        activity,
        "TabPage::PaintNow()",
        TraceLoggingValue("No tabview", "Result"));
      return;
    }
  }

  auto metrics = GetPageMetrics();
  auto tab = mTabView->GetTab();
  if (tab->GetPageCount()) {
    OPENKNEEBOARD_TraceLoggingScope("TabPage/RenderPage");
    tab->RenderPage(
      mRenderTarget.get(), mTabView->GetPageID(), metrics.mRenderRect);
  } else {
    auto d2d = mRenderTarget->d2d();
    mErrorRenderer->Render(
      d2d, _("No Pages"), metrics.mRenderRect, mForegroundBrush.get());
  }

  if (!mDrawCursor) {
    TraceLoggingWriteStop(
      activity,
      "TabPage::PaintNow()",
      TraceLoggingValue("RenderedNoCursor", "Result"),
      TraceLoggingValue(tab->GetPageCount(), "PageCount"));
    return;
  }

  auto maybePoint = mKneeboardView->GetCursorContentPoint();
  if (!maybePoint) {
    TraceLoggingWriteStop(
      activity,
      "TabPage::PaintNow()",
      TraceLoggingValue("RenderedNoCursorPoint", "Result"),
      TraceLoggingValue(tab->GetPageCount(), "PageCount"));
    return;
  }
  auto point = *maybePoint;
  point.x *= metrics.mRenderSize.width;
  point.y *= metrics.mRenderSize.height;
  point.x += metrics.mRenderRect.left;
  point.y += metrics.mRenderRect.top;
  {
    auto d2d = mRenderTarget->d2d();
    mCursorRenderer->Render(d2d, point, metrics.mRenderSize);
  }
  TraceLoggingWriteStop(
    activity,
    "TabPage::PaintNow()",
    TraceLoggingValue("RenderedWithCursor", "Result"),
    TraceLoggingValue(tab->GetPageCount(), "PageCount"));
}

TabPage::PageMetrics TabPage::GetPageMetrics() {
  if (!mTabView) {
    throw std::logic_error("Attempt to fetch Page Metrics without a tab");
  }
  const auto preferredSize = mTabView->GetPageIDs().size() == 0 ?
    PreferredSize {PixelSize {
      static_cast<uint32_t>(mCanvasSize.width),
      static_cast<uint32_t>(mCanvasSize.height),
    },
    }
  : mTabView->GetPreferredSize();

  const auto& contentNativeSize = preferredSize.mPixelSize;

  const bool unscaled = preferredSize.mScalingKind == ScalingKind::Bitmap
    && contentNativeSize.mWidth <= mCanvasSize.width
    && contentNativeSize.mHeight <= mCanvasSize.height;

  const auto scaleX = mCanvasSize.width / contentNativeSize.mWidth;
  const auto scaleY = mCanvasSize.height / contentNativeSize.mHeight;
  const auto scale = unscaled ? 1.0f : std::min(scaleX, scaleY);

  const D2D1_SIZE_F contentRenderSize {
    contentNativeSize.mWidth * scale,
    contentNativeSize.mHeight * scale,
  };

  // Use floor() to pixel-align raster sources
  const auto padX
    = std::floor((mCanvasSize.width - contentRenderSize.width) / 2);
  const auto padY
    = std::floor((mCanvasSize.height - contentRenderSize.height) / 2);

  const D2D1_RECT_F contentRenderRect {
    .left = padX,
    .top = padY,
    .right = padX + contentRenderSize.width,
    .bottom = padY + contentRenderSize.height,
  };

  return {contentNativeSize, contentRenderRect, contentRenderSize};
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

  x *= mCompositionScaleX;
  y *= mCompositionScaleY;

  x -= metrics.mRenderRect.left;
  y -= metrics.mRenderRect.top;
  x /= metrics.mRenderSize.width;
  y /= metrics.mRenderSize.height;

  auto ppp = pp.Properties();

  const bool leftClick = ppp.IsLeftButtonPressed();
  const bool rightClick = ppp.IsRightButtonPressed();

  if (x < 0 || x > 1 || y < 0 || y > 1) {
    mKneeboardView->PostCursorEvent({});
    return;
  }

  const auto canvasPoint = mKneeboardView->GetCursorCanvasPoint({x, y});

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
