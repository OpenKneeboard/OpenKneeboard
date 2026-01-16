// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "TabPage.xaml.h"
#include "TabPage.g.cpp"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/CreateTabActions.hpp>
#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/CursorRenderer.hpp>
#include <OpenKneeboard/D2DErrorRenderer.hpp>
#include <OpenKneeboard/ICheckableToolbarItem.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/IToolbarFlyout.hpp>
#include <OpenKneeboard/IToolbarItemWithConfirmation.hpp>
#include <OpenKneeboard/IToolbarItemWithVisibility.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/ToolbarSeparator.hpp>
#include <OpenKneeboard/ToolbarToggleAction.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <microsoft.ui.xaml.media.dxinterop.h>

#include <mutex>
#include <ranges>
#include <source_location>

using namespace ::OpenKneeboard;

namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxcp = winrt::Microsoft::UI::Xaml::Controls::Primitives;

namespace winrt::OpenKneeboardApp::implementation {

std::unordered_map<TabView::RuntimeID, std::weak_ptr<RenderTarget>>
  TabPage::sRenderTarget;

std::shared_ptr<RenderTarget> TabPage::GetRenderTarget(
  const audited_ptr<DXResources>& dxr,
  const TabView::RuntimeID& key) {
  // We can have multiple TabPages for the same Tab at the same time,
  // e.g. when alternating views
  //
  // We don't want switching views (i.e. looking around in VR) to force
  // uncached renders, so:
  //
  // 1. We need to preserve the resources
  // 2. We need to keep the RenderTarget, so that we have the same
  //   RenderTargetID, so the tabs themselves can maintain their caches
  if (!sRenderTarget.contains(key)) {
    auto ret = RenderTarget::Create(dxr, nullptr);
    sRenderTarget.emplace(key, ret);
    return ret;
  }

  auto& it = sRenderTarget.at(key);
  if (auto ret = it.lock()) {
    return ret;
  }

  auto ret = RenderTarget::Create(dxr, nullptr);
  it = ret;
  return ret;
}

TabPage::TabPage() {
  InitializeComponent();
  OPENKNEEBOARD_TraceLoggingScope("TabPage::TabPage()");
  mDXR.copy_from(gDXResources);
  mKneeboard = gKneeboard.lock();

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
    mKneeboard->evFrameTimerPreEvent,
    {
      get_weak(),
      [](auto self) {
        self->UpdateKneeboardView();
        if (self->mHaveCursorEvents) {
          self->FlushCursorEvents();
        }
      },
    });
}

TabPage::~TabPage() {
  OPENKNEEBOARD_TraceLoggingScope("TabPage::~TabPage()");
}

OpenKneeboard::fire_and_forget TabPage::final_release(
  std::unique_ptr<TabPage> instance) {
  OPENKNEEBOARD_TraceLoggingCoro("TabPage::final_release()");
  instance->RemoveAllEventListeners();
  co_await instance->mUIThread;
}

void TabPage::InitializePointerSource() {
  mDQC = DispatcherQueueController::CreateOnDedicatedThread();
  mDQC.DispatcherQueue().TryEnqueue([weak = get_weak()]() {
    auto self = weak.get();
    if (!self) {
      return;
    }
    SetThreadDescription(
      GetCurrentThread(), L"OKB TabPage IndependentInputSource");

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
        self->EnqueueCursorEvent({});
      }
    });
  });
}

void TabPage::OnNavigatedTo(const NavigationEventArgs&) noexcept {
  this->UpdateKneeboardView();
  this->PaintLater();
}

void TabPage::UpdateKneeboardView() {
  auto view = mKneeboard->GetAppWindowView();
  if (mKneeboardView == view) {
    this->SetTab(mKneeboardView->GetCurrentTabView());
    return;
  }
  for (const auto& event: mKneeboardViewEvents) {
    this->RemoveEventListener(event);
  }

  mKneeboardView = view;
  mKneeboardViewEvents = {
    AddEventListener(
      mKneeboardView->evCursorEvent,
      [this](const auto& ev) {
        if (ev.mSource == CursorSource::WindowPointer) {
          mDrawCursor = false;
        } else {
          mDrawCursor = ev.mTouchState != CursorTouchState::NotNearSurface;
          PaintLater();
        }
      }),
  };

  this->SetTab(mKneeboardView->GetCurrentTabView());
};

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
  button.Checked(
    &ToolbarToggleAction::Activate | task_bind_refs_front(action)
    | bind_enqueue(mKneeboard.get()) | drop_winrt_event_args());
  button.Unchecked(
    &ToolbarToggleAction::Deactivate | task_bind_refs_front(action)
    | bind_enqueue(mKneeboard.get()) | drop_winrt_event_args());

  AddEventListener(
    action->evStateChangedEvent,
    [](auto action, auto button) noexcept {
      button.IsChecked(action->IsActive());
      button.IsEnabled(action->IsEnabled());
    } | bind_refs_front(action, button)
      | bind_winrt_context(mUIThread));

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
    [](auto action, auto button) noexcept {
      button.IsEnabled(action->IsEnabled());
    } | bind_refs_front(action, button)
      | bind_winrt_context(mUIThread));

  return button;
}

task<void> TabPage::OnToolbarActionClick(
  std::shared_ptr<ToolbarAction> action) {
  auto confirm
    = std::dynamic_pointer_cast<IToolbarItemWithConfirmation>(action);
  if (!confirm) {
    co_await action->Execute();
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

  co_await action->Execute();
}

muxc::AppBarButton TabPage::CreateAppBarButton(
  const std::shared_ptr<ToolbarAction>& action) {
  auto button = CreateAppBarButtonBase(action);
  button.Click(
    &TabPage::OnToolbarActionClick | task_bind_refs_front(this, action)
    | bind_enqueue(mKneeboard.get()) | drop_winrt_event_args());
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
      [](auto tmfi, auto checkable) { tmfi.IsChecked(checkable->IsChecked()); }
        | bind_refs_front(tmfi, checkable) | bind_winrt_context(mUIThread));
  } else {
    ret = {};
  }
  ret.Text(winrt::to_hstring(action->GetLabel()));
  ret.IsEnabled(action->IsEnabled());
  ret.Click(
    &TabPage::OnToolbarActionClick | task_bind_refs_front(this, action)
    | bind_enqueue(mKneeboard.get()) | drop_winrt_event_args());

  AddEventListener(
    action->evStateChangedEvent,
    [](auto action, auto item) noexcept { item.IsEnabled(action->IsEnabled()); }
      | bind_refs_front(action, ret) | bind_winrt_context(mUIThread));
  return ret;
}

muxc::AppBarButton TabPage::CreateAppBarFlyout(
  const std::shared_ptr<IToolbarFlyout>& item) {
  // TODO (WinUI upgrade): there should be chevrons for these, but aren't
  // even when done in the XAML. Report bug if still present in latest
  auto button = CreateAppBarButtonBase(item);

  muxc::MenuFlyout flyout;
  for (auto& subitem: item->GetSubItems()) {
    const auto flyoutItem = CreateMenuFlyoutItem(subitem);
    if (flyoutItem) {
      AttachVisibility(subitem, flyoutItem);
      flyout.Items().Append(flyoutItem);
    }
  }

  button.Flyout(flyout);

  return button;
}

void TabPage::SetTab(const std::shared_ptr<TabView>& state) {
  static const TabView::RuntimeID FakeViewForErrors {};
  if (state == mTabView) {
    if (!mRenderTarget) {
      mRenderTarget = GetRenderTarget(mDXR, FakeViewForErrors);
    }
    return;
  }
  if (mTabView) {
    this->RemoveEventListener(this->mTabViewRepaintToken);
  }

  mTabView = state;
  if (state) {
    mRenderTarget = GetRenderTarget(mDXR, state->GetRuntimeID());
    mTabViewRepaintToken = AddEventListener(
      state->evNeedsRepaintEvent, {this, &TabPage::PaintLater});
  } else {
    mRenderTarget = GetRenderTarget(mDXR, FakeViewForErrors);
  }
  this->PaintLater();

  this->UpdateToolbar();
}

OpenKneeboard::fire_and_forget TabPage::UpdateToolbar() {
  co_await mUIThread;

  auto primary = CommandBar().PrimaryCommands();
  auto secondary = CommandBar().SecondaryCommands();
  primary.Clear();
  secondary.Clear();

  if (!mTabView) {
    co_return;
  }

  auto actions
    = InAppActions::Create(mKneeboard.get(), mKneeboardView, mTabView);
  {
    std::vector keepAlive = actions.mPrimary;
    keepAlive.reserve(keepAlive.size() + actions.mSecondary.size());
    std::ranges::copy(actions.mSecondary, std::back_inserter(keepAlive));
    mToolbarItems = keepAlive;
  }

  for (const auto& item: actions.mPrimary) {
    auto element = CreateCommandBarElement(item);
    if (element) {
      AttachVisibility(item, element);
      primary.Append(element);
    }
  }

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
    [](auto control, auto visibility) {
      control.Visibility(
        visibility->IsVisible() ? Visibility::Visible : Visibility::Collapsed);
    } | bind_refs_front(control, visibility)
      | bind_winrt_context(mUIThread));
}

void TabPage::OnCanvasSizeChanged(
  const IInspectable&,
  const SizeChangedEventArgs& args) noexcept {
  auto size = args.NewSize();
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity,
    "TabPage::OnCanvasSizeChanged()",
    TraceLoggingPointer(this, "this"),
    TraceLoggingValue(size.Width, "Width"),
    TraceLoggingValue(size.Height, "Height"));

  // We don't use the WinUI composition scale as
  // we render on a real pixel/percentage basis, not a DIP basis
  const auto panelDimensions = Geometry2D::Size<double>(size.Width, size.Height)
                                 .Rounded<uint32_t, PixelSize>();
  if (panelDimensions == mPanelDimensions) {
    activity.StopWithResult("SameSize");
    return;
  }
  mPanelDimensions = panelDimensions;
  this->PaintLater();
}

void TabPage::ResizeSwapChain() {
  OPENKNEEBOARD_TraceLoggingScope("TabPage::ResizeSwapChain()");

  // Can't resize swapchain if there are any reference to its' members
  mCanvas = {};
  mRenderTarget->SetD3DTexture(nullptr);

  DXGI_SWAP_CHAIN_DESC desc;
  winrt::check_hresult(mSwapChain->GetDesc(&desc));
  winrt::check_hresult(mSwapChain->ResizeBuffers(
    desc.BufferCount,
    mPanelDimensions.mWidth,
    mPanelDimensions.mHeight,
    desc.BufferDesc.Format,
    desc.Flags));

  winrt::check_hresult(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mCanvas.put())));
  mRenderTarget->SetD3DTexture(mCanvas);
  mSwapChainDimensions = mPanelDimensions;
}

void TabPage::InitializeSwapChain() {
  OPENKNEEBOARD_TraceLoggingScope(
    "TabPage::InitializeSwapChain()",
    TraceLoggingPointer(this, "this"),
    TraceLoggingValue(mPanelDimensions.mWidth, "Width"),
    TraceLoggingValue(mPanelDimensions.mHeight, "Height"));
  if (mShuttingDown) {
    return;
  }

  if (mPanelDimensions == mSwapChainDimensions) {
    return;
  }
  // BufferCount = 3: triple-buffer to avoid stalls
  //
  // If the previous frame is still being Present()ed and we
  // only have two frames in the buffer, Present()ing the new
  // frame will stall until that has completed.
  //
  // We could avoid this by using frame pacing, but we want to decouple the
  // frame rates - if you're on a 30hz or 60hz monitor, OpenKneeboard should
  // still be able to push VR frames at 90hz
  //
  // So, triple-buffer
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc {
    .Width = mPanelDimensions.mWidth,
    .Height = mPanelDimensions.mHeight,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = 3,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
  };
  winrt::check_hresult(mDXR->mDXGIFactory->CreateSwapChainForComposition(
    mDXR->mDXGIDevice.get(), &swapChainDesc, nullptr, mSwapChain.put()));

  winrt::check_hresult(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mCanvas.put())));
  Canvas().as<ISwapChainPanelNative>()->SetSwapChain(mSwapChain.get());
  mSwapChainDimensions = mPanelDimensions;
}

void TabPage::PaintLater() {
  TraceLoggingWrite(gTraceProvider, "TabPage::PaintLater()");
  mNeedsFrame = true;
  mKneeboard->SetRepaintNeeded();
}

task<void> TabPage::PaintNow(std::source_location loc) noexcept {
  if (!mTabView) {
    OPENKNEEBOARD_TraceLoggingWrite("TabPage::PaintNow()/NoTabView");
    co_return;
  }
  const auto rootTab = mTabView->GetRootTab().lock();
  if (!rootTab) {
    OPENKNEEBOARD_TraceLoggingWrite("TabPage::PaintNow()/StaleTab");
    co_return;
  }
  OPENKNEEBOARD_TraceLoggingCoro(
    /*activity,*/
    "TabPage::PaintNow()",
    TraceLoggingPointer(this, "this"),
    TraceLoggingValue(rootTab->GetTitle().c_str(), "TabTitle"),
    TraceLoggingValue(
      to_hstring(rootTab->GetPersistentID()).c_str(), "TabGUID"),
    OPENKNEEBOARD_TraceLoggingSourceLocation(loc));
  // TODO: support 'stop with result' for coros
  if (!mPanelDimensions) {
    co_return;
  }

  if (!mSwapChain) {
    this->InitializeSwapChain();
    if (!mSwapChain) {
      // activity.StopWithResult("No swap chain");
      co_return;
    }
  }
  if (mPanelDimensions != mSwapChainDimensions) {
    this->ResizeSwapChain();
  }

  const std::unique_lock lock(*mDXR);
  const auto cleanup = scope_exit([this]() {
    OPENKNEEBOARD_TraceLoggingScope("TabPage/Present()");
    mSwapChain->Present(0, 0);
    mNeedsFrame = false;
  });

  mRenderTarget->SetD3DTexture(mCanvas);

  {
    auto ctx = mRenderTarget->d2d();
    ctx->Clear(mBackgroundColor);

    if (!mTabView) {
      D3D11_TEXTURE2D_DESC desc;
      mCanvas->GetDesc(&desc);
      mErrorRenderer->Render(
        ctx,
        _("No Active Tab"),
        {
          0,
          0,
          static_cast<FLOAT>(desc.Width),
          static_cast<FLOAT>(desc.Height),
        });
      // activity.StopWithResult("No TabView");
      co_return;
    }
  }

  auto metrics = GetPageMetrics();
  if (!metrics.mRenderSize) {
    auto ctx = mRenderTarget->d2d();
    D3D11_TEXTURE2D_DESC desc;
    mCanvas->GetDesc(&desc);
    mErrorRenderer->Render(
      ctx,
      _("Render width or height is 0"),
      {
        0,
        0,
        static_cast<FLOAT>(desc.Width),
        static_cast<FLOAT>(desc.Height),
      });
    // activity.StopWithResult("Render width or height is 0");
    co_return;
  }
  auto tab = mTabView->GetTab().lock();
  if (!tab) {
    co_return;
  }
  if (tab->GetPageCount()) {
    OPENKNEEBOARD_TraceLoggingCoro("TabPage/RenderPage");
    co_await tab->RenderPage(
      RenderContext {
        mRenderTarget.get(),
        mKneeboardView.get(),
      },
      mTabView->GetPageID(),
      metrics.mRenderRect);
  } else {
    mErrorRenderer->Render(
      mRenderTarget->d2d(),
      _("No Pages"),
      metrics.mRenderRect,
      mForegroundBrush.get());
  }

  if (!mDrawCursor) {
    // activity.StopWithResult("RenderedWithoutCursor");
    co_return;
  }

  auto maybePoint = mKneeboardView->GetCursorContentPoint();
  if (!maybePoint) {
    // activity.StopWithResult("RenderedWithoutCursorPoint");
    co_return;
  }
  Geometry2D::Point<float> point {maybePoint->x, maybePoint->y};
  point.mX *= metrics.mRenderSize.Width();
  point.mY *= metrics.mRenderSize.Height();
  point.mX += metrics.mRenderRect.Left();
  point.mY += metrics.mRenderRect.Top();
  {
    auto d2d = mRenderTarget->d2d();
    mCursorRenderer->Render(
      d2d, point.Rounded<uint32_t>(), metrics.mRenderSize);
  }
  // activity.StopWithResult("RenderedWithCursor");
}

TabPage::PageMetrics TabPage::GetPageMetrics() {
  OPENKNEEBOARD_ASSERT(
    mTabView, "Attempt to fetch Page Metrics without a TabView");

  const auto preferredSize
    = mTabView->GetPreferredSize().value_or(PreferredSize {mPanelDimensions});

  const auto& contentNativeSize = preferredSize.mPixelSize;

  const auto contentRenderSize
    = contentNativeSize.StaticCast<float>()
        .ScaledToFit(mPanelDimensions.StaticCast<float>())
        .Rounded<uint32_t>();

  // Use floor() to pixel-align raster sources
  const auto padX = static_cast<uint32_t>(
    std::floor((mPanelDimensions.mWidth - contentRenderSize.mWidth) / 2.0));
  const auto padY = static_cast<uint32_t>(
    std::floor((mPanelDimensions.mHeight - contentRenderSize.mHeight) / 2.0));

  const PixelRect contentRenderRect {{padX, padY}, contentRenderSize};

  return {contentNativeSize, contentRenderRect, contentRenderSize};
}

void TabPage::OnPointerEvent(
  const IInspectable&,
  const PointerEventArgs& args) noexcept {
  for (auto pp: args.GetIntermediatePoints()) {
    this->EnqueuePointerPoint(pp);
  }
  auto pp = args.CurrentPoint();
  this->EnqueuePointerPoint(pp);
}

void TabPage::FlushCursorEvents() {
  mThreadGuard.CheckThread();
  std::unique_lock lock(mCursorEventsMutex);
  while (!mCursorEvents.empty()) {
    mKneeboardView->PostCursorEvent(mCursorEvents.front());
    mCursorEvents.pop();
  }
  mHaveCursorEvents = false;
}

void TabPage::EnqueuePointerPoint(const PointerPoint& pp) {
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

  x -= metrics.mRenderRect.Left();
  y -= metrics.mRenderRect.Top();
  x /= metrics.mRenderSize.Width();
  y /= metrics.mRenderSize.Height();

  auto ppp = pp.Properties();

  const bool leftClick = ppp.IsLeftButtonPressed();
  const bool rightClick = ppp.IsRightButtonPressed();

  if (x < 0 || x > 1 || y < 0 || y > 1) {
    this->EnqueueCursorEvent({});
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

  this->EnqueueCursorEvent(
    CursorEvent {
      .mSource = CursorSource::WindowPointer,
      .mTouchState = (leftClick || rightClick)
        ? CursorTouchState::TouchingSurface
        : CursorTouchState::NearSurface,
      .mX = canvasPoint.x,
      .mY = canvasPoint.y,
      .mPressure = rightClick ? 0.8f : 0.0f,
      .mButtons = buttons,
    });
}

void TabPage::EnqueueCursorEvent(const CursorEvent& ev) {
  std::unique_lock lock(mCursorEventsMutex);
  mCursorEvents.push(ev);
  mHaveCursorEvents = true;
}

}// namespace winrt::OpenKneeboardApp::implementation
