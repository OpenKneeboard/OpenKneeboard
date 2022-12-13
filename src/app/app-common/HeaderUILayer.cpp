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
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/ITabView.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/utf8.h>

#include <algorithm>

namespace OpenKneeboard {

HeaderUILayer::HeaderUILayer(const DXResources& dxr, KneeboardState* kneeboard)
  : mDXResources(dxr), mKneeboard(kneeboard) {
  auto ctx = dxr.mD2DDeviceContext;

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
  if (!mLastRenderSize) {
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

IUILayer::CoordinateMapping HeaderUILayer::GetCoordinateMapping(
  const IUILayer::NextList& next,
  const Context& context) const {
  const auto nextMapping
    = next.front()->GetCoordinateMapping(next.subspan(1), context);

  const auto headerHeight
    = nextMapping.mCanvasSize.height * (HeaderPercent / 100.0f);
  return {
    {
      nextMapping.mCanvasSize.width,
      nextMapping.mCanvasSize.height + headerHeight,
    },
    {
      nextMapping.mContentArea.left,
      nextMapping.mContentArea.top + headerHeight,
      nextMapping.mContentArea.right,
      nextMapping.mContentArea.bottom + headerHeight,
    },
  };
}

void HeaderUILayer::Render(
  const IUILayer::NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  const auto tabView = context.mTabView;

  const auto totalHeight = rect.bottom - rect.top;
  const auto preferredSize
    = this->GetCoordinateMapping(next, context).mCanvasSize;

  const constexpr auto totalHeightRatio = 1 + (HeaderPercent / 100.0f);
  const auto contentHeight = totalHeight / totalHeightRatio;

  const D2D1_SIZE_F headerSize {
    rect.right - rect.left,
    totalHeight - contentHeight,
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
  this->DrawHeaderText(tabView, d2d, headerRect, headerSize);
  this->DrawToolbar(context, d2d, rect, headerRect, headerSize);

  next.front()->Render(
    next.subspan(1),
    context,
    d2d,
    {rect.left, rect.top + headerSize.height, rect.right, rect.bottom});
}

void HeaderUILayer::DrawToolbar(
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& fullRect,
  const D2D1_RECT_F& headerRect,
  const D2D1_SIZE_F& headerSize) {
  if (!context.mIsActiveForInput) {
    return;
  }
  this->LayoutToolbar(context, fullRect, headerRect, headerSize);
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
  const D2D1_SIZE_F& headerSize) {
  const auto& tabView = context.mTabView;

  if (
    mToolbar && tabView && tabView == mToolbar->mTabView.lock()
    && mToolbar->mRect == fullRect) {
    return;
  }

  mToolbar = {};
  if (!tabView) {
    return;
  }

  const auto& kneeboardView = context.mKneeboardView;
  auto allActions = CreateTabActions(mKneeboard, kneeboardView, tabView);
  std::vector<std::shared_ptr<TabAction>> actions;

  std::ranges::copy_if(
    allActions, std::back_inserter(actions), [](const auto& action) {
      return action->GetVisibility(TabAction::Context::InGameToolbar)
        == TabAction::Visibility::Primary;
    });
  std::ranges::copy_if(
    allActions | std::ranges::views::reverse,
    std::back_inserter(actions),
    [](const auto& action) {
      return action->GetVisibility(TabAction::Context::InGameToolbar)
        == TabAction::Visibility::Secondary;
    });
  allActions.clear();

  std::vector<Button> buttons;

  const auto buttonHeight = headerSize.height * 0.75f;
  const auto margin = (headerSize.height - buttonHeight) / 2.0f;

  auto primaryLeft = headerRect.left + (2 * margin);
  auto secondaryRight = headerRect.right - (2 * margin);

  for (const auto& action: actions) {
    const auto visibility
      = action->GetVisibility(TabAction::Context::InGameToolbar);
    if (visibility == TabAction::Visibility::None) [[unlikely]] {
      throw std::logic_error("Should not have been copied to actions local");
    }

    AddEventListener(action->evStateChangedEvent, this->evNeedsRepaintEvent);

    D2D1_RECT_F button {
      .top = margin,
      .bottom = margin + buttonHeight,
    };
    if (visibility == TabAction::Visibility::Primary) {
      button.left = primaryLeft;
      button.right = primaryLeft + buttonHeight,
      primaryLeft = button.right + margin;
    } else {
      button.right = secondaryRight;
      button.left = secondaryRight - buttonHeight;
      secondaryRight = button.left - margin;
    }
    buttons.push_back({button, action});
  }

  auto toolbar = std::make_unique<CursorClickableRegions<Button>>(buttons);
  AddEventListener(toolbar->evClicked, [](auto, const Button& button) {
    button.mAction->Execute();
  });

  mToolbar = {
    .mTabView = tabView,
    .mRect = fullRect,
    .mButtons = std::move(toolbar),
  };
}

void HeaderUILayer::DrawHeaderText(
  const std::shared_ptr<ITabView>& tabView,
  ID2D1DeviceContext* ctx,
  const D2D1_RECT_F& headerRect,
  const D2D1_SIZE_F& headerSize) const {
  const auto tab = tabView ? tabView->GetRootTab().get() : nullptr;
  const auto title = tab ? winrt::to_hstring(tab->GetTitle()) : _(L"No Tab");
  auto& dwf = mDXResources.mDWriteFactory;

  FLOAT dpix, dpiy;
  ctx->GetDpi(&dpix, &dpiy);
  winrt::com_ptr<IDWriteTextFormat> headerFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    (headerSize.height * 96) / (2 * dpiy),
    L"",
    headerFormat.put()));

  winrt::com_ptr<IDWriteTextLayout> headerLayout;
  winrt::check_hresult(dwf->CreateTextLayout(
    title.data(),
    static_cast<UINT32>(title.size()),
    headerFormat.get(),
    float(headerSize.width),
    float(headerSize.height),
    headerLayout.put()));
  headerLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  headerLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  ctx->DrawTextLayout({0.0f, 0.0f}, headerLayout.get(), mHeaderTextBrush.get());
}

bool HeaderUILayer::Button::operator==(const Button& other) const noexcept {
  return mAction == other.mAction;
}

}// namespace OpenKneeboard
