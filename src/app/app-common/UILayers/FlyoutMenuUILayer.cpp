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
#include <OpenKneeboard/ConfirmationUILayer.h>
#include <OpenKneeboard/FlyoutMenuUILayer.h>
#include <OpenKneeboard/ICheckableToolbarItem.h>
#include <OpenKneeboard/ISelectableToolbarItem.h>
#include <OpenKneeboard/IToolbarFlyout.h>
#include <OpenKneeboard/IToolbarItemWithConfirmation.h>
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/ToolbarSeparator.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/scope_guard.h>
#include <dwrite.h>

static bool operator==(const D2D1_RECT_F& a, const D2D1_RECT_F& b) {
  return memcmp(&a, &b, sizeof(D2D1_RECT_F)) == 0;
}

namespace OpenKneeboard {

std::shared_ptr<FlyoutMenuUILayer> FlyoutMenuUILayer::Create(
  const DXResources& dxr,
  const std::vector<std::shared_ptr<IToolbarItem>>& items,
  D2D1_POINT_2F preferredTopLeft01,
  D2D1_POINT_2F preferredTopRight01,
  Corner preferredCorner) {
  return std::shared_ptr<FlyoutMenuUILayer>(new FlyoutMenuUILayer(
    dxr, items, preferredTopLeft01, preferredTopRight01, preferredCorner));
}

FlyoutMenuUILayer::FlyoutMenuUILayer(
  const DXResources& dxr,
  const std::vector<std::shared_ptr<IToolbarItem>>& items,
  D2D1_POINT_2F preferredTopLeft01,
  D2D1_POINT_2F preferredTopRight01,
  Corner preferredAnchor)
  : mDXResources(dxr),
    mItems(items),
    mPreferredTopLeft01(preferredTopLeft01),
    mPreferredTopRight01(preferredTopRight01),
    mPreferredAnchor(preferredAnchor) {
  auto ctx = dxr.mD2DDeviceContext;
  ctx->CreateSolidColorBrush(
    {1.0f, 1.0f, 1.0f, 0.6f}, D2D1::BrushProperties(), mBGOverpaintBrush.put());
  ctx->CreateSolidColorBrush(
    {0.8f, 0.8f, 0.8f, 0.8f}, D2D1::BrushProperties(), mMenuBGBrush.put());
  ctx->CreateSolidColorBrush(
    {0.0f, 0.8f, 1.0f, 1.0f}, D2D1::BrushProperties(), mMenuHoverBGBrush.put());
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f}, D2D1::BrushProperties(), mMenuFGBrush.put());
  ctx->CreateSolidColorBrush(
    {0.4f, 0.4f, 0.4f, 0.5f},
    D2D1::BrushProperties(),
    reinterpret_cast<ID2D1SolidColorBrush**>(mMenuDisabledFGBrush.put()));
}

FlyoutMenuUILayer::~FlyoutMenuUILayer() {
  RemoveAllEventListeners();
}

void FlyoutMenuUILayer::PostCursorEvent(
  const NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  if (!(mMenu && mLastRenderRect)) {
    next.front()->PostCursorEvent(
      next.subspan(1), context, eventContext, cursorEvent);
    return;
  }

  if (mPrevious) {
    mPrevious->PostCursorEvent(next, context, eventContext, cursorEvent);
    return;
  }

  CursorEvent menuEvent {cursorEvent};
  menuEvent.mX *= (mLastRenderRect->right - mLastRenderRect->left);
  menuEvent.mY *= (mLastRenderRect->bottom - mLastRenderRect->top);
  mMenu->mCursorImpl->PostCursorEvent(eventContext, menuEvent);

  evNeedsRepaintEvent.Emit();
}

IUILayer::Metrics FlyoutMenuUILayer::GetMetrics(
  const NextList& next,
  const Context& context) const {
  return next.front()->GetMetrics(next.subspan(1), context);
}

void FlyoutMenuUILayer::Render(
  const NextList& next,
  const Context& context,
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect) {
  if (mPrevious && !mRecursiveCall) {
    mRecursiveCall = true;
    const scope_guard endRecursive([this]() { mRecursiveCall = false; });

    std::vector<IUILayer*> submenuNext {this};
    submenuNext.reserve(next.size() + 1);
    std::ranges::copy(next, std::back_inserter(submenuNext));

    mPrevious->Render(submenuNext, context, d2d, rect);
    return;
  }

  if ((!mLastRenderRect) || rect != mLastRenderRect) {
    this->UpdateLayout(d2d, rect);
    if (!mMenu) {
      OPENKNEEBOARD_BREAK;
      return;
    }
  }

  if (!next.empty()) {
    next.front()->Render(next.subspan(1), context, d2d, rect);
  }

  Menu menu {};
  try {
    menu = mMenu.value();
  } catch (const std::bad_optional_access&) {
    return;
  }

  d2d->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
  const scope_guard popClip([d2d]() { d2d->PopAxisAlignedClip(); });

  if (!mPrevious) {
    d2d->FillRectangle(rect, mBGOverpaintBrush.get());
  }

  d2d->FillRoundedRectangle(
    D2D1::RoundedRect(menu.mRect, menu.mMargin, menu.mMargin),
    mMenuBGBrush.get());

  auto dwf = mDXResources.mDWriteFactory;

  std::wstring chevron {L"\ue76c"};// ChevronRight
  std::wstring checkmark {L"\ue73e"};// CheckMark

  auto [hoverMenuItem, menuItems] = menu.mCursorImpl->GetState();

  for (const auto& menuItem: menuItems) {
    auto selectable
      = std::dynamic_pointer_cast<ISelectableToolbarItem>(menuItem.mItem);
    if (!selectable) {
      continue;
    }

    if (menuItem == hoverMenuItem && selectable->IsEnabled()) {
      d2d->FillRectangle(menuItem.mRect, mMenuHoverBGBrush.get());
    }

    auto fgBrush = selectable->IsEnabled() ? mMenuFGBrush.get()
                                           : mMenuDisabledFGBrush.get();

    d2d->DrawTextW(
      menuItem.mLabel.data(),
      menuItem.mLabel.size(),
      menu.mTextFormat.get(),
      menuItem.mLabelRect,
      fgBrush);

    auto submenu = std::dynamic_pointer_cast<IToolbarFlyout>(menuItem.mItem);
    if (submenu) {
      d2d->DrawTextW(
        chevron.data(),
        chevron.size(),
        menu.mGlyphFormat.get(),
        menuItem.mChevronRect,
        fgBrush);
    }

    auto checkable
      = std::dynamic_pointer_cast<ICheckableToolbarItem>(menuItem.mItem);
    if (checkable && checkable->IsChecked()) {
      d2d->DrawTextW(
        checkmark.data(),
        checkmark.size(),
        menu.mGlyphFormat.get(),
        menuItem.mGlyphRect,
        fgBrush);
    } else if (!checkable) {
      auto glyph = menuItem.mGlyph;
      if (!glyph.empty()) {
        d2d->DrawTextW(
          glyph.data(),
          glyph.size(),
          menu.mGlyphFormat.get(),
          menuItem.mGlyphRect,
          fgBrush);
      }
    }
  }

  for (const auto& rect: menu.mSeparatorRects) {
    const auto y = rect.top + ((rect.bottom - rect.top) / 2) - 1;
    D2D1_POINT_2F left {rect.left + (menu.mMargin * 2), y};
    D2D1_POINT_2F right {rect.right - (menu.mMargin * 2), y};
    d2d->DrawLine(left, right, mMenuFGBrush.get());
  }
}

void FlyoutMenuUILayer::UpdateLayout(
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& renderRect) {
  mLastRenderRect = renderRect;
  const D2D1_SIZE_F canvasSize = {
    renderRect.right - renderRect.left,
    renderRect.bottom - renderRect.top,
  };
  const D2D1_SIZE_F maxMenuSize = {
    canvasSize.width / 2,
    canvasSize.height,
  };

  // 1. How much space do we need?

  const auto selectableItemHeight
    = canvasSize.height * 0.5f * (HeaderPercent / 100.0f);
  const auto textHeight = selectableItemHeight * 0.67f;
  const auto separatorHeight = selectableItemHeight;

  FLOAT dpix, dpiy;
  d2d->GetDpi(&dpix, &dpiy);
  auto dwf = mDXResources.mDWriteFactory;

  const auto fontSize = textHeight * 96 / dpiy;

  winrt::com_ptr<IDWriteTextFormat> textFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    fontSize,
    L"en-us",
    textFormat.put()));
  textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  winrt::com_ptr<IDWriteTextFormat> glyphFormat;
  winrt::check_hresult(dwf->CreateTextFormat(
    GlyphFont,
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    fontSize,
    L"en-us",
    glyphFormat.put()));
  glyphFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  glyphFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

  float totalHeight = 0;
  float maxTextWidth = 0;
  bool haveChevron = false;
  bool haveGlyphOrCheck = false;

  for (const auto& item: mItems) {
    if (std::dynamic_pointer_cast<ToolbarSeparator>(item)) {
      totalHeight += separatorHeight;
      continue;
    }
    auto selectable = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      continue;
    }

    totalHeight += selectableItemHeight;

    if ((!haveChevron) && std::dynamic_pointer_cast<IToolbarFlyout>(item)) {
      haveChevron = true;
    }
    if (
      (!haveGlyphOrCheck)
      && std::dynamic_pointer_cast<ICheckableToolbarItem>(item)) {
      haveGlyphOrCheck = true;
    }

    if (!selectable->GetGlyph().empty()) {
      haveGlyphOrCheck = true;
    }
    winrt::com_ptr<IDWriteTextLayout> layout;
    auto label = winrt::to_hstring(selectable->GetLabel());
    auto inf = std::numeric_limits<FLOAT>::infinity();
    winrt::check_hresult(dwf->CreateTextLayout(
      label.data(), label.size(), textFormat.get(), inf, inf, layout.put()));
    DWRITE_TEXT_METRICS metrics {};
    winrt::check_hresult(layout->GetMetrics(&metrics));
    auto width = metrics.width;
    if (width > maxTextWidth) {
      maxTextWidth = width;
    }
  }

  const auto margin = selectableItemHeight / 4;
  auto maxItemWidth = maxTextWidth + (2 * margin);
  if (haveGlyphOrCheck) {
    maxItemWidth += selectableItemHeight;
  }
  if (haveChevron) {
    maxItemWidth += selectableItemHeight;
  }

  maxItemWidth = std::min(maxItemWidth, maxMenuSize.width);
  maxTextWidth = maxItemWidth;
  float leftMargin = margin;
  if (haveGlyphOrCheck) {
    leftMargin += selectableItemHeight;
  }
  float rightMargin = margin;
  if (haveChevron) {
    rightMargin += selectableItemHeight;
  }
  maxTextWidth -= (leftMargin + rightMargin);
  if (maxTextWidth < 0) {
    return;
  }

  // 2. Where can we put it?

  const D2D1_POINT_2F topLeft {
    std::max(margin, mPreferredTopLeft01.x * canvasSize.width),
    mPreferredTopLeft01.y * canvasSize.height,
  };
  const D2D1_POINT_2F topRight {
    std::min(
      (mPreferredTopRight01.x * canvasSize.width) - maxItemWidth,
      canvasSize.width - margin),
    mPreferredTopRight01.y * canvasSize.height,
  };
  const auto preferred
    = (mPreferredAnchor == Corner::TopLeft) ? topLeft : topRight;
  const auto fallback
    = (mPreferredAnchor == Corner::TopLeft) ? topRight : topLeft;
  D2D1_POINT_2F origin = preferred;
  if (origin.x < 0 || (origin.x + maxItemWidth) > canvasSize.width) {
    origin = fallback;
  }
  if (origin.x < 0 || (origin.x + maxItemWidth) > canvasSize.width) {
    origin = {0, 0};
  }

  if (origin.y + totalHeight > canvasSize.height) {
    origin.y = 0;
    // might not be enough; that's fine, just truncate
  }

  const D2D1_RECT_F menuRect {
    renderRect.left + origin.x,
    renderRect.top + origin.y,
    renderRect.left + origin.x + maxItemWidth,
    renderRect.top + origin.y + totalHeight + (2 * margin),
  };

  origin = {menuRect.left, menuRect.top + margin};

  std::vector<MenuItem> menuItems;
  std::vector<D2D1_RECT_F> separators;
  for (const auto& item: mItems) {
    if (std::dynamic_pointer_cast<ToolbarSeparator>(item)) {
      separators.push_back({
        origin.x,
        origin.y,
        origin.x + maxItemWidth,
        origin.y + separatorHeight,
      });
      origin.y += separatorHeight;

      continue;
    }

    auto selectable = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      continue;
    }
    const D2D1_RECT_F itemRect {
      origin.x,
      origin.y,
      origin.x + maxItemWidth,
      origin.y + selectableItemHeight,
    };
    const D2D1_RECT_F labelRect {
      origin.x + leftMargin,
      origin.y,
      origin.x + leftMargin + maxTextWidth + 1,// D2D considers it exclusive
      origin.y + selectableItemHeight,
    };
    const D2D1_RECT_F glyphRect {
      origin.x + margin,
      origin.y,
      origin.x + selectableItemHeight,
      origin.y + selectableItemHeight,
    };
    const D2D1_RECT_F chevronRect {
      labelRect.right + margin,
      origin.y,
      itemRect.right - margin,
      origin.y + selectableItemHeight,
    };
    origin.y += selectableItemHeight;

    menuItems.push_back({
      .mRect = itemRect,
      .mItem = item,
      .mLabel = winrt::to_hstring(selectable->GetLabel()),
      .mLabelRect = labelRect,
      .mGlyph = winrt::to_hstring(selectable->GetGlyph()),
      .mGlyphRect = glyphRect,
      .mChevronRect = chevronRect,
    });
  }

  winrt::com_ptr<IDWriteInlineObject> ellipsis;
  winrt::check_hresult(
    dwf->CreateEllipsisTrimmingSign(textFormat.get(), ellipsis.put()));
  DWRITE_TRIMMING trimming {DWRITE_TRIMMING_GRANULARITY_CHARACTER};
  winrt::check_hresult(textFormat->SetTrimming(&trimming, ellipsis.get()));

  auto cursorImpl
    = std::make_shared<CursorClickableRegions<MenuItem>>(std::move(menuItems));
  AddEventListener(
    cursorImpl->evClickedWithoutButton, [weak = weak_from_this()]() {
      if (auto self = weak.lock()) {
        self->evCloseMenuRequestedEvent.Emit();
      }
    });
  AddEventListener(
    cursorImpl->evClicked, [weak = weak_from_this()](auto, const auto& item) {
      if (auto self = weak.lock()) {
        self->OnClick(item);
      }
    });

  mMenu = {
    .mMargin = margin,
    .mRect = menuRect,
    .mCursorImpl = std::move(cursorImpl),
    .mSeparatorRects = std::move(separators),
    .mTextFormat = std::move(textFormat),
    .mGlyphFormat = std::move(glyphFormat),
  };
}

bool FlyoutMenuUILayer::MenuItem::operator==(
  const MenuItem& other) const noexcept {
  return mItem == other.mItem;
}

void FlyoutMenuUILayer::OnClick(const MenuItem& item) {
  auto checkable = std::dynamic_pointer_cast<ICheckableToolbarItem>(item.mItem);
  if (checkable && checkable->IsChecked()) {
    evCloseMenuRequestedEvent.Emit();
    return;
  }

  auto confirmable
    = std::dynamic_pointer_cast<IToolbarItemWithConfirmation>(item.mItem);
  if (confirmable) {
    auto prev
      = std::make_shared<ConfirmationUILayer>(mDXResources, confirmable);
    AddEventListener(prev->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
    AddEventListener(prev->evClosedEvent, [weak = weak_from_this()]() {
      if (auto self = weak.lock()) {
        self->mPrevious = {};
        self->evNeedsRepaintEvent.Emit();
      }
    });
    mPrevious = prev;
    evNeedsRepaintEvent.Emit();
    return;
  }

  auto action = std::dynamic_pointer_cast<ToolbarAction>(item.mItem);
  if (action) {
    action->Execute();
    evCloseMenuRequestedEvent.Emit();
    return;
  }

  auto flyout = std::dynamic_pointer_cast<IToolbarFlyout>(item.mItem);
  if (!flyout) {
    return;
  }

  auto rect = item.mRect;
  const D2D1_SIZE_F renderSize = {
    mLastRenderRect->right - mLastRenderRect->left,
    mLastRenderRect->bottom - mLastRenderRect->top,
  };

  auto subMenu = FlyoutMenuUILayer::Create(
    mDXResources,
    flyout->GetSubItems(),
    {(rect.right - mMenu->mMargin) / renderSize.width,
     rect.top / renderSize.height},// Put top-left corner here
    {(rect.left + mMenu->mMargin) / renderSize.width,
     rect.top / renderSize.height},// Put top-right corner here
    mPreferredAnchor);
  AddEventListener(
    subMenu->evCloseMenuRequestedEvent, this->evCloseMenuRequestedEvent);
  AddEventListener(subMenu->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  mPrevious = subMenu;
  evNeedsRepaintEvent.Emit();
}

}// namespace OpenKneeboard
