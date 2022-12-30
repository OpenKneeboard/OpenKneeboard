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
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/ToolbarFlyout.h>
#include <OpenKneeboard/ToolbarSeparator.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>
#include <OpenKneeboard/weak_wrap.h>

#include <algorithm>

namespace OpenKneeboard {

std::shared_ptr<HeaderUILayer> HeaderUILayer::Create(
  const DXResources& dxr,
  KneeboardState* kneeboardState,
  IKneeboardView* kneeboardView) {
  return std::shared_ptr<HeaderUILayer>(
    new HeaderUILayer(dxr, kneeboardState, kneeboardView));
}

HeaderUILayer::HeaderUILayer(
  const DXResources& dxr,
  KneeboardState* kneeboardState,
  IKneeboardView* kneeboardView)
  : mDXResources(dxr), mKneeboardState(kneeboardState) {
  auto ctx = dxr.mD2DDeviceContext;

  AddEventListener(kneeboardView->evCurrentTabChangedEvent, [this]() {
    this->mSecondaryMenu = {};
  });

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
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mActiveButtonBrush.put()));
}

HeaderUILayer::~HeaderUILayer() {
  this->RemoveAllEventListeners();
}

void HeaderUILayer::PostCursorEvent(
  const IUILayer::NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  if (cursorEvent.mSource == CursorSource::WINDOW_POINTER) {
    next.front()->PostCursorEvent(
      next.subspan(1), context, eventContext, cursorEvent);
    return;
  }

  if (!mLastRenderSize) {
    return;
  }

  if (mSecondaryMenu && !mRecursiveCall) {
    mRecursiveCall = true;
    const scope_guard endRecursive([this]() { mRecursiveCall = false; });

    std::vector<IUILayer*> menuNext {this};
    std::ranges::copy(next, std::back_inserter(menuNext));

    mSecondaryMenu->PostCursorEvent(
      menuNext, context, eventContext, cursorEvent);
    return;
  }

  const auto renderSize = *mLastRenderSize;
  if (mToolbar) {
    scope_guard repaintOnExit([this]() { evNeedsRepaintEvent.Emit(); });
    CursorEvent toolbarEvent {cursorEvent};
    toolbarEvent.mX *= renderSize.width;
    toolbarEvent.mY *= renderSize.height;
    mToolbar->mButtons->PostCursorEvent(eventContext, toolbarEvent);
  }

  constexpr auto contentRatio = 1 / (1 + (HeaderPercent / 100.0f));
  constexpr auto headerRatio = 1 - contentRatio;
  if (cursorEvent.mY <= headerRatio) {
    next.front()->PostCursorEvent(next.subspan(1), context, eventContext, {});
    return;
  }

  CursorEvent nextEvent {cursorEvent};
  nextEvent.mY = (cursorEvent.mY - headerRatio) / contentRatio;
  next.front()->PostCursorEvent(
    next.subspan(1), context, eventContext, nextEvent);
}

IUILayer::Metrics HeaderUILayer::GetMetrics(
  const IUILayer::NextList& next,
  const Context& context) const {
  const auto nextMetrics = next.front()->GetMetrics(next.subspan(1), context);

  const auto contentHeight
    = nextMetrics.mContentArea.bottom - nextMetrics.mContentArea.top;
  const auto headerHeight = contentHeight * (HeaderPercent / 100.0f);
  return {
    {
      nextMetrics.mCanvasSize.width,
      nextMetrics.mCanvasSize.height + headerHeight,
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
  const IUILayer::NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  const auto tabView = context.mTabView;

  const auto metrics = this->GetMetrics(next, context);
  const auto preferredSize = metrics.mCanvasSize;

  const auto totalHeight = rect.bottom - rect.top;
  const auto scale = totalHeight / preferredSize.height;

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
  d2d->SetTransform(D2D1::Matrix3x2F::Identity());
  d2d->FillRectangle(headerRect, mHeaderBGBrush.get());
  auto headerTextRect = headerRect;
  this->DrawToolbar(
    context, d2d, rect, headerRect, headerSize, &headerTextRect);
  this->DrawHeaderText(tabView, d2d, headerTextRect);

  next.front()->Render(
    next.subspan(1),
    context,
    d2d,
    {rect.left, rect.top + headerSize.height, rect.right, rect.bottom});

  if (mSecondaryMenu) {
    mSecondaryMenu->Render({}, context, d2d, rect);
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
  if (!mToolbar) {
    return;
  }

  auto toolbar = mToolbar->mButtons.get();

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
  winrt::check_hresult(mDXResources.mDWriteFactory->CreateTextFormat(
    L"Segoe MDL2 Assets",
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
      auto toggle = std::dynamic_pointer_cast<TabToggleAction>(action);
      if (toggle && toggle->IsActive()) {
        brush = mActiveButtonBrush.get();
      }
    }

    d2d->DrawRoundedRectangle(
      D2D1::RoundedRect(button.mRect, buttonHeight / 4, buttonHeight / 4),
      brush,
      strokeWidth);
    auto glyph = winrt::to_hstring(action->GetGlyph());
    d2d->DrawTextW(
      glyph.c_str(), glyph.size(), glyphFormat.get(), button.mRect, brush);
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

  if (
    mToolbar && tabView && tabView == mToolbar->mTabView.lock()
    && mToolbar->mRect == fullRect) {
    *headerTextRect = mToolbar->mTextRect;
    return;
  }

  mToolbar = {};
  if (!tabView) {
    return;
  }

  const auto& kneeboardView = context.mKneeboardView;
  const auto actions
    = InGameActions::Create(mKneeboardState, kneeboardView, tabView);
  std::vector<Button> buttons;

  const auto buttonHeight = headerSize.height * 0.75f;
  const auto margin = (headerSize.height - buttonHeight) / 2.0f;

  auto primaryLeft = headerRect.left + (2 * margin);

  for (const auto& item: actions.mLeft) {
    const auto selectable
      = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      OPENKNEEBOARD_BREAK;
      continue;
    }
    AddEventListener(
      selectable->evStateChangedEvent, this->evNeedsRepaintEvent);

    D2D1_RECT_F button {
      .top = margin,
      .bottom = margin + buttonHeight,
    };
    button.left = primaryLeft;
    button.right = primaryLeft + buttonHeight,
    primaryLeft = button.right + margin;

    buttons.push_back({button, selectable});
  }

  auto secondaryRight = headerRect.right - (2 * margin);
  for (const auto& item: actions.mRight) {
    const auto selectable
      = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      OPENKNEEBOARD_BREAK;
      continue;
    }
    AddEventListener(
      selectable->evStateChangedEvent, this->evNeedsRepaintEvent);

    D2D1_RECT_F button {
      .top = margin,
      .bottom = margin + buttonHeight,
    };
    button.right = secondaryRight;
    button.left = secondaryRight - buttonHeight;
    secondaryRight = button.left - margin;

    buttons.push_back({button, selectable});
  }

  auto toolbar = std::make_unique<CursorClickableRegions<Button>>(buttons);
  AddEventListener(
    toolbar->evClicked, [weak = weak_from_this()](auto, const Button& button) {
      auto self = weak.lock();
      if (auto self = weak.lock()) {
        self->OnClick(button);
      }
    });

  *headerTextRect = {
    primaryLeft,
    headerRect.top,
    secondaryRight,
    headerRect.bottom,
  };

  mToolbar = {
    .mTabView = tabView,
    .mRect = fullRect,
    .mTextRect = *headerTextRect,
    .mButtons = std::move(toolbar),
  };
}

void HeaderUILayer::DrawHeaderText(
  const std::shared_ptr<ITabView>& tabView,
  ID2D1DeviceContext* ctx,
  const D2D1_RECT_F& textRect) const {
  const auto tab = tabView ? tabView->GetRootTab().get() : nullptr;
  const auto title = tab ? winrt::to_hstring(tab->GetTitle()) : _(L"No Tab");
  auto& dwf = mDXResources.mDWriteFactory;

  const D2D1_SIZE_F textSize {
    textRect.right - textRect.left,
    textRect.bottom - textRect.top,
  };

  FLOAT dpix, dpiy;
  ctx->GetDpi(&dpix, &dpiy);
  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    L"Consolas",
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

  if (mSecondaryMenu) {
    mSecondaryMenu.reset();
    evNeedsRepaintEvent.Emit();
    return;
  }
  mSecondaryMenu = FlyoutMenuUILayer::Create(
    mDXResources,
    flyout->GetSubItems(),
    D2D1_POINT_2F {0.0f, HeaderPercent / 100.0f},
    D2D1_POINT_2F {1.0f, HeaderPercent / 100.0f},
    FlyoutMenuUILayer::Corner::TopRight);
  AddEventListener(
    mSecondaryMenu->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  AddEventListener(
    mSecondaryMenu->evCloseMenuRequestedEvent,
    weak_wrap(
      [](auto self) {
        self->mSecondaryMenu = {};
        self->evNeedsRepaintEvent.Emit();
      },
      this));
  evNeedsRepaintEvent.Emit();
}

}// namespace OpenKneeboard
