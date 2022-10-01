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
#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/UserAction.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <ranges>

namespace OpenKneeboard {

KneeboardView::KneeboardView(const DXResources& dxr, KneeboardState* kneeboard)
  : mDXR(dxr), mKneeboard(kneeboard) {
  UpdateLayout();
}

KneeboardView::~KneeboardView() {
  this->RemoveAllEventListeners();
}

KneeboardViewID KneeboardView::GetRuntimeID() const {
  return mID;
}

void KneeboardView::SetTabs(const std::vector<std::shared_ptr<ITab>>& tabs) {
  if (std::ranges::equal(tabs, mTabViews, {}, {}, &ITabView::GetTab)) {
    return;
  }

  decltype(mTabViews) viewStates;

  for (const auto& tab: tabs) {
    auto it = std::ranges::find(mTabViews, tab, &ITabView::GetTab);
    if (it != mTabViews.end()) {
      viewStates.push_back(*it);
      continue;
    }

    auto viewState = std::make_shared<TabView>(mDXR, mKneeboard, tab);
    viewStates.push_back(viewState);

    AddEventListener(viewState->evNeedsRepaintEvent, [=]() {
      if (viewState == this->GetCurrentTabView()) {
        this->evNeedsRepaintEvent.Emit();
      }
    });
    AddEventListener(tab->evAvailableFeaturesChangedEvent, [=]() {
      if (viewState == this->GetCurrentTabView()) {
        this->evNeedsRepaintEvent.Emit();
      }
    });
    AddEventListener(
      viewState->evPageChangedEvent, &KneeboardView::UpdateLayout, this);
  }

  mTabViews = viewStates;
  auto it = std::ranges::find(viewStates, mCurrentTabView);
  if (it == viewStates.end()) {
    mCurrentTabView = tabs.empty() ? nullptr : viewStates.front();
  }

  UpdateLayout();
}

TabIndex KneeboardView::GetTabIndex() const {
  auto it = std::ranges::find(mTabViews, mCurrentTabView);
  if (it == mTabViews.end()) {
    return 0;
  }
  return static_cast<TabIndex>(it - mTabViews.begin());
}

void KneeboardView::SetCurrentTabByIndex(TabIndex index) {
  if (index >= mTabViews.size()) {
    return;
  }
  if (mCurrentTabView == mTabViews.at(index)) {
    return;
  }
  if (mCurrentTabView) {
    mCurrentTabView->PostCursorEvent({});
  }
  mCurrentTabView = mTabViews.at(index);
  UpdateLayout();
  evCurrentTabChangedEvent.Emit(index);
}

void KneeboardView::SetCurrentTabByID(ITab::RuntimeID id) {
  const auto it = std::ranges::find(mTabViews, id, [](const auto& view) {
    return view->GetRootTab()->GetRuntimeID();
  });

  if (it == mTabViews.end()) {
    return;
  }
  if (*it == mCurrentTabView) {
    return;
  }

  SetCurrentTabByIndex(it - mTabViews.begin());
}

void KneeboardView::PreviousTab() {
  const auto count = mTabViews.size();
  if (count < 2) {
    return;
  }

  const auto current = GetTabIndex();
  if (current > 0) {
    SetCurrentTabByIndex(current - 1);
    return;
  }

  if (mKneeboard->GetAppSettings().mLoopTabs) {
    SetCurrentTabByIndex(count - 1);
  }
}

void KneeboardView::NextTab() {
  const auto count = mTabViews.size();
  if (count < 2) {
    return;
  }

  const auto current = GetTabIndex();
  if (current + 1 < count) {
    SetCurrentTabByIndex(current + 1);
    return;
  }

  if (mKneeboard->GetAppSettings().mLoopTabs) {
    SetCurrentTabByIndex(0);
    return;
  }
}

std::shared_ptr<ITabView> KneeboardView::GetTabViewByID(
  ITab::RuntimeID id) const {
  for (const auto& tabView: mTabViews) {
    if (tabView->GetTab()->GetRuntimeID() == id) {
      return tabView;
    }
    if (tabView->GetRootTab()->GetRuntimeID() == id) {
      return tabView;
    }
  }
  dprint("Failed to find tab by ID");
  OPENKNEEBOARD_BREAK;
  return {};
}

std::shared_ptr<ITab> KneeboardView::GetCurrentTab() const {
  if (!mCurrentTabView) [[unlikely]] {
    return {};
  }
  return mCurrentTabView->GetTab();
}

std::shared_ptr<ITabView> KneeboardView::GetCurrentTabView() const {
  return mCurrentTabView;
}

void KneeboardView::NextPage() {
  if (mCurrentTabView) {
    mCurrentTabView->NextPage();
    UpdateLayout();
  }
}

void KneeboardView::PreviousPage() {
  if (mCurrentTabView) {
    mCurrentTabView->PreviousPage();
    UpdateLayout();
  }
}

void KneeboardView::UpdateLayout() {
  const auto totalHeightRatio = 1 + (HeaderPercent / 100.0f);

  mContentNativeSize = {768, 1024};
  auto tab = GetCurrentTabView();
  if (tab && tab->GetPageCount() > 0) {
    mContentNativeSize = tab->GetNativeContentSize();
  }

  if (mContentNativeSize.width == 0 || mContentNativeSize.height == 0) {
    mContentNativeSize = {768, 1024};
  }

  const auto scaleX
    = static_cast<float>(TextureWidth) / mContentNativeSize.width;
  const auto scaleY = static_cast<float>(TextureHeight)
    / (totalHeightRatio * mContentNativeSize.height);
  const auto scale = std::min(scaleX, scaleY);
  const D2D1_SIZE_F contentRenderSize {
    mContentNativeSize.width * scale,
    mContentNativeSize.height * scale,
  };
  const D2D1_SIZE_F headerRenderSize {
    static_cast<FLOAT>(contentRenderSize.width),
    contentRenderSize.height * (HeaderPercent / 100.0f),
  };

  mCanvasSize = {
    static_cast<UINT>(contentRenderSize.width),
    static_cast<UINT>(contentRenderSize.height + headerRenderSize.height),
  };
  mHeaderRenderRect = {
    .left = 0,
    .top = 0,
    .right = headerRenderSize.width,
    .bottom = headerRenderSize.height,
  };
  mContentRenderRect = {
    .left = 0,
    .top = mHeaderRenderRect.bottom,
    .right = contentRenderSize.width,
    .bottom = mHeaderRenderRect.bottom + contentRenderSize.height,
  };

  evLayoutChangedEvent.Emit();
  evNeedsRepaintEvent.Emit();
}

const D2D1_SIZE_U& KneeboardView::GetCanvasSize() const {
  return mCanvasSize;
}

const D2D1_SIZE_U& KneeboardView::GetContentNativeSize() const {
  return mContentNativeSize;
}

const D2D1_RECT_F& KneeboardView::GetHeaderRenderRect() const {
  return mHeaderRenderRect;
}

const D2D1_RECT_F& KneeboardView::GetContentRenderRect() const {
  return mContentRenderRect;
}

void KneeboardView::PostCursorEvent(const CursorEvent& ev) {
  mCursorPoint = {ev.mX, ev.mY};
  mHaveCursor = ev.mTouchState != CursorTouchState::NOT_NEAR_SURFACE;

  if (mCurrentTabView) {
    mCurrentTabView->PostCursorEvent(ev);
  }

  evCursorEvent.Emit(ev);
}

void KneeboardView::PostUserAction(UserAction action) {
  switch (action) {
    case UserAction::PREVIOUS_TAB:
      this->PreviousTab();
      return;
    case UserAction::NEXT_TAB:
      this->NextTab();
      return;
    case UserAction::PREVIOUS_PAGE:
      this->PreviousPage();
      return;
    case UserAction::NEXT_PAGE:
      this->NextPage();
      return;
    case UserAction::TOGGLE_VISIBILITY:
    case UserAction::TOGGLE_FORCE_ZOOM:
    case UserAction::RECENTER_VR:
      // Handled by KneeboardState
      return;
  }
  OPENKNEEBOARD_BREAK;
}

bool KneeboardView::HaveCursor() const {
  return mHaveCursor;
}

D2D1_POINT_2F KneeboardView::GetCursorPoint() const {
  return mCursorPoint;
}

D2D1_POINT_2F KneeboardView::GetCursorCanvasPoint() const {
  return this->GetCursorCanvasPoint(this->GetCursorPoint());
}

D2D1_POINT_2F KneeboardView::GetCursorCanvasPoint(
  const D2D1_POINT_2F& contentPoint) const {
  const auto contentRect = this->GetContentRenderRect();
  const D2D1_SIZE_F contentSize = {
    contentRect.right - contentRect.left,
    contentRect.bottom - contentRect.top,
  };
  const auto contentNativeSize = this->GetContentNativeSize();
  const auto scale = contentSize.height / contentNativeSize.height;

  auto cursorPoint = contentPoint;
  cursorPoint.x *= scale;
  cursorPoint.y *= scale;
  cursorPoint.x += contentRect.left;
  cursorPoint.y += contentRect.top;
  return cursorPoint;
}

}// namespace OpenKneeboard
