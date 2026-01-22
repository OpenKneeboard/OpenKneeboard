// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/CreateTabActions.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/HeaderUILayer.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/IToolbarItemWithVisibility.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabletInputAdapter.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/ToolbarFlyout.hpp>
#include <OpenKneeboard/ToolbarSeparator.hpp>
#include <OpenKneeboard/ToolbarToggleAction.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/utf8.hpp>

#include <algorithm>

namespace OpenKneeboard {

std::shared_ptr<HeaderUILayer> HeaderUILayer::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboardState,
  KneeboardView* kneeboardView) {
  return std::shared_ptr<HeaderUILayer>(
    new HeaderUILayer(dxr, kneeboardState, kneeboardView));
}

HeaderUILayer::HeaderUILayer(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboardState,
  KneeboardView* kneeboardView)
  : mDXResources(dxr),
    mKneeboardState(kneeboardState) {
  auto ctx = dxr->mD2DDeviceContext;

  AddEventListener(
    kneeboardView->evCurrentTabChangedEvent,
    std::bind_front(&HeaderUILayer::OnTabChanged, this));

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHeaderBGBrush.put()));
  mHeaderTextBrush = dxr->mBlackBrush;
  ctx->CreateSolidColorBrush(
    {0.4f, 0.4f, 0.4f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mDisabledButtonBrush.put()));
  mButtonBrush = dxr->mBlackBrush;
  ctx->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHoverButtonBrush.put()));
  mActiveButtonBrush = mHoverButtonBrush;
  mActiveViewSeparatorBrush = mDXResources->mBlackBrush;
}

HeaderUILayer::~HeaderUILayer() { this->RemoveAllEventListeners(); }

void HeaderUILayer::OnTabChanged() {
  mToolbar.reset();

  mSecondaryMenu.reset();
  for (const auto& event: mTabEvents) {
    this->RemoveEventListener(event);
  }
  mTabEvents.clear();

  evNeedsRepaintEvent.Emit();
}

void HeaderUILayer::PostCursorEvent(
  const IUILayer::NextList& next,
  const Context& context,
  KneeboardViewID KneeboardViewID,
  const CursorEvent& cursorEvent) {
  if (cursorEvent.mSource == CursorSource::WindowPointer) {
    this->PostNextCursorEvent(next, context, KneeboardViewID, cursorEvent);
    return;
  }

  if (!mLastRenderSize) {
    return;
  }

  auto secondaryMenu = mSecondaryMenu;
  if (secondaryMenu && !mRecursiveCall) {
    mRecursiveCall = true;
    const scope_exit endRecursive([this]() { mRecursiveCall = false; });

    std::vector<IUILayer*> menuNext {this};
    std::ranges::copy(next, std::back_inserter(menuNext));

    secondaryMenu->PostCursorEvent(
      menuNext, context, KneeboardViewID, cursorEvent);
    return;
  }

  const auto renderSize = *mLastRenderSize;
  auto toolbar = mToolbar;
  if (toolbar && toolbar->mButtons) {
    scope_exit repaintOnExit([this]() { evNeedsRepaintEvent.Emit(); });
    CursorEvent toolbarEvent {cursorEvent};
    toolbarEvent.mX *= renderSize.mWidth;
    toolbarEvent.mY *= renderSize.mHeight;
    toolbar->mButtons->PostCursorEvent(KneeboardViewID, toolbarEvent);
  }

  this->PostNextCursorEvent(next, context, KneeboardViewID, cursorEvent);
}

IUILayer::Metrics HeaderUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  OPENKNEEBOARD_TraceLoggingScope("HeaderUILayer::GetMetrics()");
  const auto nextMetrics = next.front()->GetMetrics(next.subspan(1), context);

  const auto contentHeight = nextMetrics.mContentArea.Height();
  const auto headerHeight = static_cast<uint32_t>(
    std::lround(contentHeight * (HeaderPercent / 100.0f)));
  return Metrics {
    nextMetrics.mPreferredSize.Extended({0, headerHeight}),
    {
      {0, headerHeight},
      nextMetrics.mPreferredSize.mPixelSize,
    },
    {
      nextMetrics.mContentArea.mOffset + PixelPoint {0, headerHeight},
      nextMetrics.mContentArea.mSize,
    },
  };
}

task<void> HeaderUILayer::Render(
  const RenderContext& rc,
  const IUILayer::NextList& next,
  const Context& context,
  const PixelRect& rect) {
  OPENKNEEBOARD_TraceLoggingScope("HeaderUILayer::Render()");
  const auto tabView = context.mTabView;

  const auto metrics = this->GetMetrics(next, context);
  const auto preferredSize = metrics.mPreferredSize;

  const auto scale = rect.Height<float>() / preferredSize.mPixelSize.mHeight;

  const auto contentHeight =
    static_cast<uint32_t>(std::lround(scale * metrics.mContentArea.Height()));
  const auto headerHeight = static_cast<uint32_t>(
    std::lround(contentHeight * (HeaderPercent / 100.0f)));

  const PixelRect headerRect {
    rect.mOffset,
    {rect.Width(), headerHeight},
  };

  if (headerRect.mSize.IsEmpty()) {
    co_return;
  }

  mLastRenderSize = rect.mSize;

  {
    auto d2d = rc.d2d();
    d2d->SetTransform(D2D1::Matrix3x2F::Identity());
    d2d->FillRectangle(headerRect, mHeaderBGBrush.get());
    auto headerTextRect = headerRect;
    this->DrawToolbar(context, d2d, rect, headerRect, &headerTextRect);
    this->DrawHeaderText(tabView, d2d, headerTextRect);
  }

  co_await next.front()->Render(
    rc,
    next.subspan(1),
    context,
    {
      {rect.Left(), rect.Top() + headerHeight},
      {rect.Width(), rect.Height() - headerHeight},
    });

  auto secondaryMenu = mSecondaryMenu;
  if (secondaryMenu) {
    co_await secondaryMenu->Render(rc, {}, context, rect);
  }
}

void HeaderUILayer::DrawToolbar(
  const Context& context,
  ID2D1DeviceContext* d2d,
  const PixelRect& fullRect,
  const PixelRect& headerRect,
  PixelRect* headerTextRect) {
  if (!context.mIsActiveForInput) {
    return;
  }

  auto tablets = mKneeboardState->GetTabletInputAdapter();
  if (!(tablets && tablets->HaveAnyTablet())) {
    // If the tablet is present, the buttons indicate the active view
    d2d->DrawLine(
      {headerRect.Left<FLOAT>(), headerRect.Bottom<FLOAT>()},
      {headerRect.Right<FLOAT>(), headerRect.Bottom<FLOAT>()},
      mActiveViewSeparatorBrush.get(),
      1.0f,
      nullptr);
    return;
  }

  this->LayoutToolbar(context, fullRect, headerRect, headerTextRect);

  if (*headerTextRect == headerRect) {
    // No buttons, e.g. no tablet
    return;
  }

  auto toolbarInfo = mToolbar;
  if (!toolbarInfo) {
    return;
  }

  auto toolbar = toolbarInfo->mButtons;

  const auto [hoverButton, buttons] = toolbar->GetState();
  if (buttons.empty()) {
    return;
  }

  const auto buttonHeight =
    buttons.front().mRect.bottom - buttons.front().mRect.top;
  const auto strokeWidth = buttonHeight / 15;

  FLOAT dpix {}, dpiy {};
  d2d->GetDpi(&dpix, &dpiy);
  winrt::com_ptr<IDWriteTextFormat> glyphFormat;
  winrt::check_hresult(mDXResources->mDWriteFactory->CreateTextFormat(
    GlyphFont,
    nullptr,
    DWRITE_FONT_WEIGHT_EXTRA_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (buttonHeight * 96) * 0.66f / dpiy,
    L"en-us",
    glyphFormat.put()));
  glyphFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  glyphFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  for (auto button: buttons) {
    auto& action = button.mAction;
    ID2D1Brush* brush = mButtonBrush.get();
    if (!action->IsEnabled()) {
      brush = mDisabledButtonBrush.get();
    } else if (hoverButton == button) {
      brush = mHoverButtonBrush.get();
    } else {
      auto toggle = std::dynamic_pointer_cast<ToolbarToggleAction>(action);
      if (toggle && toggle->IsActive()) {
        brush = mActiveButtonBrush.get();
      }
    }

    auto buttonRect = button.mRect;
    buttonRect.left += headerRect.Left();
    buttonRect.top += headerRect.Top();
    buttonRect.right += headerRect.Left();
    buttonRect.bottom += headerRect.Top();

    d2d->DrawRoundedRectangle(
      D2D1::RoundedRect(buttonRect, buttonHeight / 4, buttonHeight / 4),
      brush,
      strokeWidth);
    auto glyph = winrt::to_hstring(action->GetGlyph());
    d2d->DrawTextW(
      glyph.c_str(), glyph.size(), glyphFormat.get(), buttonRect, brush);
  }
}

static bool operator==(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept {
  return memcmp(&a, &b, sizeof(D2D1_RECT_F)) == 0;
}

void HeaderUILayer::LayoutToolbar(
  const Context& context,
  const PixelRect& fullRect,
  const PixelRect& headerRect,
  PixelRect* headerTextRect) {
  const auto& tabView = context.mTabView;

  auto tablets = mKneeboardState->GetTabletInputAdapter();
  if (!(tablets && tablets->HaveAnyTablet())) {
    *headerTextRect = headerRect;
    mToolbar = nullptr;
    return;
  }

  if (mTabEvents.empty()) {
    mTabEvents = {
      this->AddEventListener(
        tabView->evAvailableFeaturesChangedEvent,
        {weak_from_this(), &HeaderUILayer::OnTabChanged}),
    };
  }

  auto toolbar = mToolbar;

  if (
    toolbar && tabView && tabView == toolbar->mTabView.lock()
    && toolbar->mRect == fullRect) {
    const auto& r = toolbar->mTextRect;
    *headerTextRect = Geometry2D::Rect<float>(
                        {r.left, r.top}, {r.right - r.left, r.bottom - r.top})
                        .Rounded<uint32_t>();
    return;
  }

  mToolbar.reset();
  if (!tabView) {
    return;
  }

  const auto& kneeboardView = context.mKneeboardView;
  const auto actions =
    InGameActions::Create(mKneeboardState, kneeboardView, tabView);
  std::vector<Button> buttons;

  const auto buttonHeight =
    static_cast<uint32_t>(std::lround(headerRect.Height() * 0.75f));
  const auto margin = (headerRect.Height() - buttonHeight) / 2;

  auto primaryLeft = 2 * margin;

  auto resetToolbar = [weak = weak_from_this()]() {
    if (auto self = weak.lock()) {
      self->OnTabChanged();
    }
  };

  for (const auto& item: actions.mLeft) {
    const auto selectable =
      std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      OPENKNEEBOARD_BREAK;
      continue;
    }
    AddEventListener(selectable->evStateChangedEvent, resetToolbar);

    const auto visibility =
      std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
    if (visibility && !visibility->IsVisible()) {
      continue;
    }

    D2D1_RECT_F button {
      .top = static_cast<float>(margin),
      .bottom = static_cast<float>(margin + buttonHeight),
    };
    button.left = felly::numeric_cast<float>(primaryLeft);
    button.right = felly::numeric_cast<float>(primaryLeft + buttonHeight),
    primaryLeft += buttonHeight + margin;

    buttons.push_back({button, selectable});
  }

  auto secondaryRight = headerRect.Width() - (2 * margin);
  for (const auto& item: actions.mRight) {
    const auto selectable =
      std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      OPENKNEEBOARD_BREAK;
      continue;
    }
    AddEventListener(selectable->evStateChangedEvent, resetToolbar);

    const auto visibility =
      std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
    if (visibility && !visibility->IsVisible()) {
      continue;
    }

    D2D1_RECT_F button {
      .top = static_cast<float>(margin),
      .bottom = static_cast<float>(margin + buttonHeight),
    };
    button.right = static_cast<float>(secondaryRight);
    button.left = static_cast<float>(secondaryRight - buttonHeight);
    secondaryRight -= buttonHeight + margin;

    buttons.push_back({button, selectable});
  }

  auto toolbarHandler = CursorClickableRegions<Button>::Create(buttons);
  AddEventListener(
    toolbarHandler->evClicked,
    [weak = weak_from_this()](auto, const Button& button) {
      if (auto self = weak.lock()) {
        self->OnClick(button);
      }
    });

  const auto buttonSpace =
    std::max<uint32_t>(primaryLeft, headerRect.Width() - secondaryRight);

  *headerTextRect = {
    headerRect.mOffset + PixelPoint {primaryLeft, 0},
    {
      headerRect.Width() - (2 * buttonSpace),
      headerRect.Height(),
    },
  };

  if (headerTextRect->Left() > headerTextRect->Right()) {
    *headerTextRect = {};
  }

  mToolbar.reset(new Toolbar {
    .mTabView = tabView,
    .mRect = fullRect,
    .mTextRect = *headerTextRect,
    .mButtons = std::move(toolbarHandler),
  });
}// namespace OpenKneeboard

void HeaderUILayer::DrawHeaderText(
  const std::shared_ptr<TabView>& tabView,
  ID2D1DeviceContext* ctx,
  const PixelRect& textRect) const {
  const auto& textSize = textRect.mSize;

  if (textSize == PixelSize {}) {
    return;
  }

  const std::shared_ptr<ITab> tab =
    tabView ? tabView->GetRootTab().lock() : nullptr;
  const auto title = tab ? winrt::to_hstring(tab->GetTitle()) : _(L"No Tab");
  auto& dwf = mDXResources->mDWriteFactory;

  FLOAT dpix {}, dpiy {};
  ctx->GetDpi(&dpix, &dpiy);
  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    FixedWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (textSize.mHeight * 96) / (2 * dpiy),
    L"",
    headerFormat.put()));
  winrt::com_ptr<IDWriteInlineObject> ellipsis;
  winrt::check_hresult(
    dwf->CreateEllipsisTrimmingSign(headerFormat.get(), ellipsis.put()));
  DWRITE_TRIMMING trimming {
    .granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER};
  winrt::check_hresult(headerFormat->SetTrimming(&trimming, ellipsis.get()));

  winrt::com_ptr<IDWriteTextLayout> headerLayout;
  winrt::check_hresult(dwf->CreateTextLayout(
    title.data(),
    static_cast<UINT32>(title.size()),
    headerFormat.get(),
    textSize.Width<float>(),
    textSize.Height<float>(),
    headerLayout.put()));
  headerLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  headerLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  ctx->DrawTextLayout(
    textRect.TopLeft(), headerLayout.get(), mHeaderTextBrush.get());
}

bool HeaderUILayer::Button::operator==(const Button& other) const noexcept {
  return mAction == other.mAction;
}

void HeaderUILayer::OnClick(const Button& button) {
  auto action = std::dynamic_pointer_cast<ToolbarAction>(button.mAction);
  if (action) {
    fire_and_forget::wrap(&ToolbarAction::Execute, action);
    return;
  }

  auto flyout = std::dynamic_pointer_cast<IToolbarFlyout>(button.mAction);
  if (!flyout) {
    return;
  }

  auto secondaryMenu = mSecondaryMenu;
  if (secondaryMenu) {
    secondaryMenu.reset();
    evNeedsRepaintEvent.Emit();
    return;
  }
  secondaryMenu = FlyoutMenuUILayer::Create(
    mDXResources,
    flyout->GetSubItems(),
    D2D1_POINT_2F {0.0f, HeaderPercent / 100.0f},
    D2D1_POINT_2F {1.0f, HeaderPercent / 100.0f},
    FlyoutMenuUILayer::Corner::TopRight);
  AddEventListener(
    secondaryMenu->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    secondaryMenu->evCloseMenuRequestedEvent,
    {
      weak_from_this(),
      [](auto self) {
        self->mSecondaryMenu = {};
        self->evNeedsRepaintEvent.Emit();
      },
    });
  mSecondaryMenu = secondaryMenu;
  evNeedsRepaintEvent.Emit();
}

}// namespace OpenKneeboard
