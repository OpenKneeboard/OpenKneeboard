// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ConfirmationUILayer.hpp>
#include <OpenKneeboard/FlyoutMenuUILayer.hpp>
#include <OpenKneeboard/ICheckableToolbarItem.hpp>
#include <OpenKneeboard/ISelectableToolbarItem.hpp>
#include <OpenKneeboard/IToolbarFlyout.hpp>
#include <OpenKneeboard/IToolbarItemWithConfirmation.hpp>
#include <OpenKneeboard/IToolbarItemWithVisibility.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/ToolbarSeparator.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <dwrite.h>

using felly::numeric_cast;

namespace OpenKneeboard {

std::shared_ptr<FlyoutMenuUILayer> FlyoutMenuUILayer::Create(
  const audited_ptr<DXResources>& dxr,
  const std::vector<std::shared_ptr<IToolbarItem>>& items,
  D2D1_POINT_2F preferredTopLeft01,
  D2D1_POINT_2F preferredTopRight01,
  Corner preferredCorner) {
  return std::shared_ptr<FlyoutMenuUILayer>(new FlyoutMenuUILayer(
    dxr, items, preferredTopLeft01, preferredTopRight01, preferredCorner));
}

FlyoutMenuUILayer::FlyoutMenuUILayer(
  const audited_ptr<DXResources>& dxr,
  const std::vector<std::shared_ptr<IToolbarItem>>& items,
  D2D1_POINT_2F preferredTopLeft01,
  D2D1_POINT_2F preferredTopRight01,
  Corner preferredAnchor)
  : mDXResources(dxr),
    mItems(items),
    mPreferredTopLeft01(preferredTopLeft01),
    mPreferredTopRight01(preferredTopRight01),
    mPreferredAnchor(preferredAnchor) {
  auto ctx = dxr->mD2DDeviceContext;
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

  for (const auto& item: mItems) {
    auto visibility =
      std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
    if (visibility) {
      AddEventListener(
        visibility->evStateChangedEvent, this->evNeedsRepaintEvent);
    }
  }
}

FlyoutMenuUILayer::~FlyoutMenuUILayer() { RemoveAllEventListeners(); }

void FlyoutMenuUILayer::PostCursorEvent(
  const NextList& next,
  const Context& context,
  KneeboardViewID KneeboardViewID,
  const CursorEvent& cursorEvent) {
  if (!(mMenu && mLastRenderRect)) {
    next.front()->PostCursorEvent(
      next.subspan(1), context, KneeboardViewID, cursorEvent);
    return;
  }

  auto previous = mPrevious;
  if (previous) {
    previous->PostCursorEvent(next, context, KneeboardViewID, cursorEvent);
    return;
  }

  CursorEvent menuEvent {cursorEvent};
  menuEvent.mX *= mLastRenderRect->Width();
  menuEvent.mY *= mLastRenderRect->Height();
  menuEvent.mX += mLastRenderRect->Left();
  menuEvent.mY += mLastRenderRect->Top();
  mMenu->mCursorImpl->PostCursorEvent(KneeboardViewID, menuEvent);

  evNeedsRepaintEvent.Emit();
}

IUILayer::Metrics FlyoutMenuUILayer::GetMetrics(
  const NextList& next,
  const Context& context) const {
  return next.front()->GetMetrics(next.subspan(1), context);
}

task<void> FlyoutMenuUILayer::Render(
  const RenderContext& rc,
  const NextList& next,
  const Context& context,
  const PixelRect& rect) {
  OPENKNEEBOARD_TraceLoggingScope("FlyoutMenuUILayer::Render()");
  auto previous = mPrevious;
  if (previous && !mRecursiveCall) {
    mRecursiveCall = true;
    const scope_exit endRecursive([this]() { mRecursiveCall = false; });

    std::vector<IUILayer*> submenuNext {this};
    submenuNext.reserve(next.size() + 1);
    std::ranges::copy(next, std::back_inserter(submenuNext));

    co_await previous->Render(rc, submenuNext, context, rect);
    co_return;
  }

  if ((!mLastRenderRect) || rect != mLastRenderRect) {
    this->UpdateLayout(rc.d2d(), rect);
    if (!mMenu) {
      OPENKNEEBOARD_BREAK;
      co_return;
    }
  }

  if (!next.empty()) {
    co_await next.front()->Render(rc, next.subspan(1), context, rect);
  }

  Menu menu {};
  try {
    menu = mMenu.value();
  } catch (const std::bad_optional_access&) {
    co_return;
  }

  auto d2d = rc.d2d();
  d2d->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_ALIASED);
  const scope_exit popClip([&d2d]() { d2d->PopAxisAlignedClip(); });

  if (!mPrevious) {
    d2d->FillRectangle(rect, mBGOverpaintBrush.get());
  }

  d2d->FillRoundedRectangle(
    D2D1::RoundedRect(menu.mRect, menu.mMargin, menu.mMargin),
    mMenuBGBrush.get());

  auto dwf = mDXResources->mDWriteFactory;

  std::wstring chevron {L"\ue76c"};// ChevronRight
  std::wstring checkmark {L"\ue73e"};// CheckMark

  auto [hoverMenuItem, menuItems] = menu.mCursorImpl->GetState();

  for (const auto& menuItem: menuItems) {
    auto selectable =
      std::dynamic_pointer_cast<ISelectableToolbarItem>(menuItem.mItem);
    if (!selectable) {
      continue;
    }

    if (menuItem == hoverMenuItem && selectable->IsEnabled()) {
      d2d->FillRectangle(menuItem.mRect, mMenuHoverBGBrush.get());
    }

    auto fgBrush =
      selectable->IsEnabled() ? mMenuFGBrush.get() : mMenuDisabledFGBrush.get();

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

    auto checkable =
      std::dynamic_pointer_cast<ICheckableToolbarItem>(menuItem.mItem);
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

  for (auto&& separator: menu.mSeparatorRects) {
    const auto y = separator.top + ((separator.bottom - separator.top) / 2) - 1;
    D2D1_POINT_2F left {separator.left + (menu.mMargin * 2), y};
    D2D1_POINT_2F right {separator.right - (menu.mMargin * 2), y};
    d2d->DrawLine(left, right, mMenuFGBrush.get());
  }
}

void FlyoutMenuUILayer::UpdateLayout(
  ID2D1DeviceContext* d2d,
  const PixelRect& renderRect) {
  mLastRenderRect = renderRect;
  const auto& canvasSize = renderRect.mSize;
  const PixelSize maxMenuSize = {
    canvasSize.mWidth / 2,
    canvasSize.mHeight,
  };

  // 1. How much space do we need?

  const auto selectableItemHeight = static_cast<uint32_t>(
    std::lround(canvasSize.mHeight * 0.5f * (HeaderPercent / 100.0f)));
  const auto textHeight = std::lround(selectableItemHeight * 0.67f);
  const auto separatorHeight = selectableItemHeight;

  FLOAT dpix {}, dpiy {};
  d2d->GetDpi(&dpix, &dpiy);
  auto dwf = mDXResources->mDWriteFactory;

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

  uint32_t totalHeight = 0;
  uint32_t maxTextWidth = 0;
  bool haveChevron = false;
  bool haveGlyphOrCheck = false;

  for (const auto& item: mItems) {
    auto visibility =
      std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
    if (visibility && !visibility->IsVisible()) {
      continue;
    }

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
    auto width = static_cast<uint32_t>(std::lround(metrics.width));
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

  maxItemWidth = std::min(maxItemWidth, maxMenuSize.mWidth);
  maxTextWidth = maxItemWidth;
  uint32_t leftMargin = margin;
  if (haveGlyphOrCheck) {
    leftMargin += selectableItemHeight;
  }
  uint32_t rightMargin = margin;
  if (haveChevron) {
    rightMargin += selectableItemHeight;
  }
  maxTextWidth -= (leftMargin + rightMargin);
  if (maxTextWidth < 0) {
    return;
  }

  // 2. Where can we put it?

  const D2D1_POINT_2F topLeft {
    renderRect.Left<FLOAT>()
      + std::max(
        numeric_cast<FLOAT>(margin), mPreferredTopLeft01.x * canvasSize.mWidth),
    renderRect.Top() + (mPreferredTopLeft01.y * canvasSize.mHeight),
  };
  const D2D1_POINT_2F topRight {
    renderRect.Left()
      + std::min(
        (mPreferredTopRight01.x * canvasSize.mWidth) - maxItemWidth,
        numeric_cast<FLOAT>(canvasSize.mWidth - margin)),
    renderRect.Top() + (mPreferredTopRight01.y * canvasSize.mHeight),
  };
  const auto preferred =
    (mPreferredAnchor == Corner::TopLeft) ? topLeft : topRight;
  const auto fallback =
    (mPreferredAnchor == Corner::TopLeft) ? topRight : topLeft;
  D2D1_POINT_2F origin = preferred;
  if (
    origin.x < renderRect.Left()
    || (origin.x + maxItemWidth) > renderRect.Right()) {
    origin = fallback;
  }
  if (
    origin.x < renderRect.Left()
    || (origin.x + maxItemWidth) > renderRect.Right()) {
    origin = renderRect.TopLeft();
  }

  if (origin.y + totalHeight > renderRect.Height()) {
    origin.y = renderRect.Top<float>();
    // might not be enough; that's fine, just truncate
  }

  const D2D1_RECT_F menuRect {
    origin.x,
    origin.y,
    origin.x + maxItemWidth,
    origin.y + totalHeight + (2 * margin),
  };

  origin = {menuRect.left, menuRect.top + margin};

  std::vector<MenuItem> menuItems;
  std::vector<D2D1_RECT_F> separators;
  for (const auto& item: mItems) {
    auto visibility =
      std::dynamic_pointer_cast<IToolbarItemWithVisibility>(item);
    if (visibility && !visibility->IsVisible()) {
      continue;
    }
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

  auto cursorImpl =
    CursorClickableRegions<MenuItem>::Create(std::move(menuItems));
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
    .mMargin = numeric_cast<FLOAT>(margin),
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

  auto confirmable =
    std::dynamic_pointer_cast<IToolbarItemWithConfirmation>(item.mItem);
  if (confirmable) {
    auto prev = ConfirmationUILayer::Create(mDXResources, confirmable);
    AddEventListener(prev->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
    AddEventListener(prev->evClosedEvent, [weak = weak_from_this()]() {
      if (auto self = weak.lock()) {
        self->mPrevious = {};
        self->evCloseMenuRequestedEvent.Emit();
      }
    });
    mPrevious = prev;
    evNeedsRepaintEvent.Emit();
    return;
  }

  auto action = std::dynamic_pointer_cast<ToolbarAction>(item.mItem);
  if (action) {
    fire_and_forget::wrap(&ToolbarAction::Execute, action);
    evCloseMenuRequestedEvent.Emit();
    return;
  }

  auto flyout = std::dynamic_pointer_cast<IToolbarFlyout>(item.mItem);
  if (!flyout) {
    return;
  }

  auto rect = item.mRect;
  const auto& renderSize = mLastRenderRect->mSize;

  auto subMenu = FlyoutMenuUILayer::Create(
    mDXResources,
    flyout->GetSubItems(),
    {
      // Top-left anchor point
      (rect.right - (mLastRenderRect->Left() + mMenu->mMargin))
        / renderSize.Width<float>(),
      (rect.top - mLastRenderRect->Top()) / renderSize.Height<float>(),
    },
    {
      // Top-right anchor point
      (rect.left + mMenu->mMargin - mLastRenderRect->Left())
        / renderSize.Width<float>(),
      (rect.top - mLastRenderRect->Top()) / renderSize.Height<float>(),
    },
    mPreferredAnchor);
  AddEventListener(
    subMenu->evCloseMenuRequestedEvent, this->evCloseMenuRequestedEvent);
  AddEventListener(subMenu->evNeedsRepaintEvent, this->evNeedsRepaintEvent);
  mPrevious = subMenu;
  evNeedsRepaintEvent.Emit();
}

}// namespace OpenKneeboard
