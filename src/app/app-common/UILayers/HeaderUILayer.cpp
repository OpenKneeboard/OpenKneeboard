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
#include <OpenKneeboard/CreateTabActions.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/HeaderUILayer.h>
#include <OpenKneeboard/IKneeboardView.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/IToolbarItemWithVisibility.h>
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/ToolbarFlyout.h>
#include <OpenKneeboard/ToolbarSeparator.h>
#include <OpenKneeboard/ToolbarToggleAction.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/weak_wrap.h>

#include <algorithm>

namespace OpenKneeboard {

std::shared_ptr<HeaderUILayer> HeaderUILayer::Create(
  const std::shared_ptr<DXResources>& dxr,
  KneeboardState* kneeboardState,
  IKneeboardView* kneeboardView) {
  return std::shared_ptr<HeaderUILayer>(
    new HeaderUILayer(dxr, kneeboardState, kneeboardView));
}

HeaderUILayer::HeaderUILayer(
  const std::shared_ptr<DXResources>& dxr,
  KneeboardState* kneeboardState,
  IKneeboardView* kneeboardView)
  : mDXResources(dxr), mKneeboardState(kneeboardState) {
  auto ctx = dxr->mD2DDeviceContext;

  AddEventListener(
    kneeboardView->evCurrentTabChangedEvent,
    std::bind_front(&HeaderUILayer::OnTabChanged, this));

  ctx->CreateSolidColorBrush(
    {0.7f, 0.7f, 0.7f, 0.8f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHeaderBGBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHeaderTextBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.4f, 0.4f, 0.4f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mDisabledButtonBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mButtonBrush.put()));
  ctx->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mHoverButtonBrush.put()));
  mActiveButtonBrush = mHoverButtonBrush;
}

HeaderUILayer::~HeaderUILayer() {
  this->RemoveAllEventListeners();
}

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
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  if (cursorEvent.mSource == CursorSource::WINDOW_POINTER) {
    this->PostNextCursorEvent(next, context, eventContext, cursorEvent);
    return;
  }

  if (!mLastRenderSize) {
    return;
  }

  auto secondaryMenu = mSecondaryMenu;
  if (secondaryMenu && !mRecursiveCall) {
    mRecursiveCall = true;
    const scope_guard endRecursive([this]() { mRecursiveCall = false; });

    std::vector<IUILayer*> menuNext {this};
    std::ranges::copy(next, std::back_inserter(menuNext));

    secondaryMenu->PostCursorEvent(
      menuNext, context, eventContext, cursorEvent);
    return;
  }

  const auto renderSize = *mLastRenderSize;
  auto toolbar = mToolbar;
  if (toolbar && toolbar->mButtons) {
    scope_guard repaintOnExit([this]() { evNeedsRepaintEvent.Emit(); });
    CursorEvent toolbarEvent {cursorEvent};
    toolbarEvent.mX *= renderSize.width;
    toolbarEvent.mY *= renderSize.height;
    toolbar->mButtons->PostCursorEvent(eventContext, toolbarEvent);
  }

  this->PostNextCursorEvent(next, context, eventContext, cursorEvent);
}

IUILayer::Metrics HeaderUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  const auto nextMetrics = next.front()->GetMetrics(next.subspan(1), context);

  const auto contentHeight
    = nextMetrics.mContentArea.bottom - nextMetrics.mContentArea.top;
  const auto headerHeight = contentHeight * (HeaderPercent / 100.0f);
  return Metrics {
    nextMetrics.mPreferredSize.Extended(
      {0, static_cast<uint32_t>(headerHeight)}),
    {
      0.0f,
      headerHeight,
      static_cast<FLOAT>(nextMetrics.mPreferredSize.mPixelSize.mWidth),
      nextMetrics.mPreferredSize.mPixelSize.mHeight + headerHeight,
    },
    {
      nextMetrics.mContentArea.left,
      nextMetrics.mContentArea.top + headerHeight,
      nextMetrics.mContentArea.right,
      nextMetrics.mContentArea.bottom + headerHeight,
    },
  };
}

void HeaderUILayer::Render(
  RenderTarget* rt,
  const IUILayer::NextList& next,
  const Context& context,
  const D2D1_RECT_F& rect) {
  const auto tabView = context.mTabView;

  const auto metrics = this->GetMetrics(next, context);
  const auto preferredSize = metrics.mPreferredSize;

  const auto totalHeight = rect.bottom - rect.top;
  const auto scale = totalHeight / preferredSize.mPixelSize.mHeight;

  const auto contentHeight
    = scale * (metrics.mContentArea.bottom - metrics.mContentArea.top);
  const auto headerHeight = contentHeight * (HeaderPercent / 100.0f);

  const D2D1_SIZE_F headerSize {
    rect.right - rect.left,
    headerHeight,
  };
  const D2D1_RECT_F headerRect {
    rect.left,
    rect.top,
    rect.right,
    rect.top + headerSize.height,
  };

  mLastRenderSize = {
    rect.right - rect.left,
    rect.bottom - rect.top,
  };

  {
    auto d2d = rt->d2d();
    d2d->SetTransform(D2D1::Matrix3x2F::Identity());
    d2d->FillRectangle(headerRect, mHeaderBGBrush.get());
    auto headerTextRect = headerRect;
    this->DrawToolbar(
      context, d2d, rect, headerRect, headerSize, &headerTextRect);
    this->DrawHeaderText(tabView, d2d, headerTextRect);
  }

  next.front()->Render(
    rt,
    next.subspan(1),
    context,
    {rect.left, rect.top + headerSize.height, rect.right, rect.bottom});

  auto secondaryMenu = mSecondaryMenu;
  if (secondaryMenu) {
    secondaryMenu->Render(rt, {}, context, rect);
  }
}

void HeaderUILayer::DrawToolbar(
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& fullRect,
  const D2D1_RECT_F& headerRect,
  const D2D1_SIZE_F& headerSize,
  D2D1_RECT_F* headerTextRect) {
  if (!context.mIsActiveForInput) {
    return;
  }

  this->LayoutToolbar(
    context, fullRect, headerRect, headerSize, headerTextRect);

  auto toolbarInfo = mToolbar;
  if (!toolbarInfo) {
    return;
  }

  auto toolbar = toolbarInfo->mButtons;

  const auto [hoverButton, buttons] = toolbar->GetState();
  if (buttons.empty()) {
    return;
  }

  const auto buttonHeight
    = buttons.front().mRect.bottom - buttons.front().mRect.top;
  const auto strokeWidth = buttonHeight / 15;

  FLOAT dpix, dpiy;
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
    buttonRect.left += headerRect.left;
    buttonRect.top += headerRect.top;
    buttonRect.right += headerRect.left;
    buttonRect.bottom += headerRect.top;

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
  const D2D1_RECT_F& fullRect,
  const D2D1_RECT_F& headerRect,
  const D2D1_SIZE_F& headerSize,
  D2D1_RECT_F* headerTextRect) {
  const auto& tabView = context.mTabView;

  if (mTabEvents.empty()) {
    mTabEvents = {
      this->AddEventListener(
        tabView->evAvailableFeaturesChangedEvent,
        weak_wrap(this)([](auto self) { self->OnTabChanged(); })),
    };
  }

  auto toolbar = mToolbar;

  if (
    toolbar && tabView && tabView == toolbar->mTabView.lock()
    && toolbar->mRect == fullRect) {
    *headerTextRect = toolbar->mTextRect;
    return;
  }

  mToolbar.reset();
  if (!tabView) {
    return;
  }

  const auto& kneeboardView = context.mKneeboardView;
  const auto actions
    = InGameActions::Create(mKneeboardState, kneeboardView, tabView);
  std::vector<Button> buttons;

  const auto buttonHeight = headerSize.height * 0.75f;
  const auto margin = (headerSize.height - buttonHeight) / 2.0f;

  auto primaryLeft = 2 * margin;

  auto resetToolbar = weak_wrap(this)([](auto self) { self->OnTabChanged(); });

  for (const auto& item: actions.mLeft) {
    const auto selectable
      = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      OPENKNEEBOARD_BREAK;
      continue;
    }
    AddEventListener(selectable->evStateChangedEvent, resetToolbar);

    const auto visibility
      = std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
    if (visibility && !visibility->IsVisible()) {
      continue;
    }

    D2D1_RECT_F button {
      .top = margin,
      .bottom = margin + buttonHeight,
    };
    button.left = primaryLeft;
    button.right = primaryLeft + buttonHeight,
    primaryLeft = button.right + margin;

    buttons.push_back({button, selectable});
  }

  auto secondaryRight = (headerRect.right - headerRect.left) - (2 * margin);
  for (const auto& item: actions.mRight) {
    const auto selectable
      = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      OPENKNEEBOARD_BREAK;
      continue;
    }
    AddEventListener(selectable->evStateChangedEvent, resetToolbar);

    const auto visibility
      = std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
    if (visibility && !visibility->IsVisible()) {
      continue;
    }

    D2D1_RECT_F button {
      .top = margin,
      .bottom = margin + buttonHeight,
    };
    button.right = secondaryRight;
    button.left = secondaryRight - buttonHeight;
    secondaryRight = button.left - margin;

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

  *headerTextRect = {
    primaryLeft + headerRect.left,
    headerRect.top,
    secondaryRight + headerRect.left,
    headerRect.bottom,
  };

  if (headerTextRect->left > headerTextRect->right) {
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
  const std::shared_ptr<ITabView>& tabView,
  ID2D1DeviceContext* ctx,
  const D2D1_RECT_F& textRect) const {
  const D2D1_SIZE_F textSize {
    textRect.right - textRect.left,
    textRect.bottom - textRect.top,
  };

  if (textSize.width <= 0.01 || textSize.height <= 0.01) {
    return;
  }

  const auto tab = tabView ? tabView->GetRootTab().get() : nullptr;
  const auto title = tab ? winrt::to_hstring(tab->GetTitle()) : _(L"No Tab");
  auto& dwf = mDXResources->mDWriteFactory;

  FLOAT dpix, dpiy;
  ctx->GetDpi(&dpix, &dpiy);
  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    FixedWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (textSize.height * 96) / (2 * dpiy),
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
    textSize.width,
    textSize.height,
    headerLayout.put()));
  headerLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  headerLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  ctx->DrawTextLayout(
    {textRect.left, textRect.top}, headerLayout.get(), mHeaderTextBrush.get());
}

bool HeaderUILayer::Button::operator==(const Button& other) const noexcept {
  return mAction == other.mAction;
}

void HeaderUILayer::OnClick(const Button& button) {
  auto action = std::dynamic_pointer_cast<ToolbarAction>(button.mAction);
  if (action) {
    action->Execute();
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
    secondaryMenu->evCloseMenuRequestedEvent, weak_wrap(this)([](auto self) {
      self->mSecondaryMenu = {};
      self->evNeedsRepaintEvent.Emit();
    }));
  mSecondaryMenu = secondaryMenu;
  evNeedsRepaintEvent.Emit();
}

}// namespace OpenKneeboard
