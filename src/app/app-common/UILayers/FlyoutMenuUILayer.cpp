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
#include <OpenKneeboard/Config.h>
#include <OpenKneeboard/FlyoutMenuUILayer.h>
#include <OpenKneeboard/ICheckableToolbarItem.h>
#include <OpenKneeboard/ISelectableToolbarItem.h>
#include <OpenKneeboard/IToolbarFlyout.h>
#include <OpenKneeboard/scope_guard.h>
#include <dwrite.h>

static bool operator==(const D2D1_RECT_F& a, const D2D1_RECT_F& b) {
  return false;
}

namespace OpenKneeboard {

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
    {0.8f, 0.8f, 0.8f, 0.8f}, D2D1::BrushProperties(), mMenuBGBrush.put());
  ctx->CreateSolidColorBrush(
    {0.0f, 0.0f, 0.0f, 1.0f}, D2D1::BrushProperties(), mMenuFGBrush.put());
}

FlyoutMenuUILayer::~FlyoutMenuUILayer() = default;

void FlyoutMenuUILayer::PostCursorEvent(
  const NextList& next,
  const Context& context,
  const EventContext& eventContext,
  const CursorEvent& cursorEvent) {
  next.front()->PostCursorEvent(
    next.subspan(1), context, eventContext, cursorEvent);
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

  d2d->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
  const scope_guard popClip([d2d]() { d2d->PopAxisAlignedClip(); });

  const auto& menu = *mMenu;
  d2d->FillRectangle(menu.mRect, mMenuBGBrush.get());
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
    canvasSize.width / 3,
    canvasSize.height,
  };

  // 1. How much space do we need?

  const auto selectableItemHeight
    = canvasSize.height * 0.8f * (HeaderPercent / 100.0f);
  const auto textHeight = selectableItemHeight * 0.67f;

  FLOAT dpix, dpiy;
  d2d->GetDpi(&dpix, &dpiy);
  winrt::com_ptr<IDWriteTextFormat> textFormat;
  auto dwf = mDXResources.mDWriteFactory;
  winrt::check_hresult(dwf->CreateTextFormat(
    L"Consolas",
    nullptr,
    DWRITE_FONT_WEIGHT_NORMAL,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    textHeight * 96 / dpiy,
    L"en-us",
    textFormat.put()));
  textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  float totalHeight = 0;
  float maxTextWidth = 0;
  bool haveChevron = false;
  bool haveGlyphOrCheck = false;
  uint32_t rowCount = 0;

  for (const auto& item: mItems) {
    auto selectable = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      continue;
    }

    if ((!haveChevron) && std::dynamic_pointer_cast<IToolbarFlyout>(item)) {
      haveChevron = true;
    }
    if (
      (!haveGlyphOrCheck)
      && std::dynamic_pointer_cast<ICheckableToolbarItem>(item)) {
      haveGlyphOrCheck = true;
    }

    totalHeight += selectableItemHeight;
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
    totalHeight += selectableItemHeight;
    rowCount++;
  }

  const auto margin = selectableItemHeight / 2;
  auto maxItemWidth = maxTextWidth + (2 * margin);
  if (haveGlyphOrCheck) {
    maxItemWidth += selectableItemHeight;
  }
  if (haveChevron) {
    maxItemWidth += selectableItemHeight;
  }

  maxItemWidth = std::min(maxItemWidth, maxMenuSize.width);
  maxTextWidth = maxItemWidth - (2 * margin);
  float leftMargin = margin;
  if (haveGlyphOrCheck) {
    leftMargin += selectableItemHeight + margin;
  }
  float rightMargin = margin;
  if (haveChevron) {
    rightMargin += selectableItemHeight + margin;
  }
  maxTextWidth -= (leftMargin + rightMargin);
  if (maxTextWidth < 0) {
    return;
  }

  // 2. Where can we put it?

  const D2D1_POINT_2F topLeft {
    mPreferredTopLeft01.x * canvasSize.width,
    mPreferredTopLeft01.y * canvasSize.height,
  };
  const D2D1_POINT_2F topRight {
    (mPreferredTopRight01.x * canvasSize.width) - maxItemWidth,
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
    renderRect.top + origin.y + totalHeight,
  };

  origin = {menuRect.left, menuRect.top};

  std::vector<MenuItem> menuItems;
  for (const auto& item: mItems) {
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
      origin.x + maxItemWidth - rightMargin,
      origin.y + selectableItemHeight,
    };
    origin.y += selectableItemHeight;

    menuItems.push_back({
      .mRect = itemRect,
      .mItem = item,
      .mLabel = winrt::to_hstring(selectable->GetLabel()),
      .mLabelRect = labelRect,
    });
  }

  mMenu = {
    .mRect = menuRect,
    .mItems
    = std::make_unique<CursorClickableRegions<MenuItem>>(std::move(menuItems)),
  };
}

bool FlyoutMenuUILayer::MenuItem::operator==(
  const MenuItem& other) const noexcept {
  return mItem == other.mItem;
}

}// namespace OpenKneeboard
