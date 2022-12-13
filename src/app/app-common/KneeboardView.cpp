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
#include <OpenKneeboard/CursorRenderer.h>
#include <OpenKneeboard/HeaderUILayer.h>
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/KneeboardView.h>
#include <OpenKneeboard/TabView.h>
#include <OpenKneeboard/TabViewUILayer.h>
#include <OpenKneeboard/UserAction.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

#include <ranges>

namespace OpenKneeboard {

KneeboardView::KneeboardView(const DXResources& dxr, KneeboardState* kneeboard)
  : mDXR(dxr), mKneeboard(kneeboard) {
  mCursorRenderer = std::make_unique<CursorRenderer>(dxr);
  mHeaderUILayer = std::make_unique<HeaderUILayer>(dxr, kneeboard);
  mTabViewUILayer = std::make_unique<TabViewUILayer>(dxr);
}

std::shared_ptr<KneeboardView> KneeboardView::Create(
  const DXResources& dxr,
  KneeboardState* kneeboard) {
  auto it = std::shared_ptr<KneeboardView>(new KneeboardView(dxr, kneeboard));
  it->UpdateLayout();
  return it;
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
    auto weakState = std::weak_ptr(viewState);

    AddEventListener(viewState->evNeedsRepaintEvent, [weakState, this]() {
      auto strongState = weakState.lock();
      if (!strongState) {
        return;
      }
      if (strongState == this->GetCurrentTabView()) {
        this->evNeedsRepaintEvent.Emit();
      }
    });
    AddEventListener(tab->evAvailableFeaturesChangedEvent, [weakState, this]() {
      auto strongState = weakState.lock();
      if (!strongState) {
        return;
      }
      if (strongState == this->GetCurrentTabView()) {
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
    this->evCurrentTabChangedEvent.Emit(this->GetTabIndex());
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

void KneeboardView::SetCurrentTabByRuntimeID(ITab::RuntimeID id) {
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
  if (mCurrentTabView) {
    mContentNativeSize = mCurrentTabView->GetNativeContentSize();
  } else {
    mContentNativeSize = {768, 1024};
  }

  std::array<IUILayer*, 1> nextLayers {mTabViewUILayer.get()};
  const auto mapping = mHeaderUILayer->GetCoordinateMapping(
    nextLayers,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView = std::static_pointer_cast<IKneeboardView>(
        std::const_pointer_cast<KneeboardView>(this->shared_from_this())),
      .mIsActiveForInput = true,
    });

  const auto canvasPreferredSize = mapping.mCanvasSize;

  const auto scaleX
    = static_cast<float>(TextureWidth) / canvasPreferredSize.width;
  const auto scaleY
    = static_cast<float>(TextureHeight) / canvasPreferredSize.height;
  const auto scale = std::min(scaleX, scaleY);

  mCanvasSize = {
    static_cast<UINT>(std::roundl(canvasPreferredSize.width * scale)),
    static_cast<UINT>(std::roundl(canvasPreferredSize.height * scale)),
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

void KneeboardView::PostCursorEvent(const CursorEvent& ev) {
  if (ev.mTouchState == CursorTouchState::NOT_NEAR_SURFACE) {
    mCursorCanvasPoint.reset();
  } else {
    mCursorCanvasPoint = {ev.mX, ev.mY};
  }

  std::array<IUILayer*, 1> nextLayers {mTabViewUILayer.get()};
  mHeaderUILayer->PostCursorEvent(
    nextLayers,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView = this->shared_from_this(),
      .mIsActiveForInput = true,
    },
    mEventContext,
    ev);

  evCursorEvent.Emit(ev);
}

void KneeboardView::RenderWithChrome(
  ID2D1DeviceContext* d2d,
  const D2D1_RECT_F& rect,
  bool isActiveForInput) noexcept {
  std::array<IUILayer*, 1> nextLayers {mTabViewUILayer.get()};
  mHeaderUILayer->Render(
    nextLayers,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView = this->shared_from_this(),
      .mIsActiveForInput = isActiveForInput,
    },
    d2d,
    rect);
  if (mCursorCanvasPoint) {
    const D2D1_SIZE_F size {
      rect.right - rect.left,
      rect.bottom - rect.top,
    };
    mCursorRenderer->Render(
      d2d,
      {
        mCursorCanvasPoint->x * size.width,
        mCursorCanvasPoint->y * size.height,
      },
      size);
  }
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

std::optional<D2D1_POINT_2F> KneeboardView::GetCursorCanvasPoint() const {
  return mCursorCanvasPoint;
}

std::optional<D2D1_POINT_2F> KneeboardView::GetCursorContentPoint() const {
  return mTabViewUILayer->GetCursorPoint();
}

D2D1_POINT_2F KneeboardView::GetCursorCanvasPoint(
  const D2D1_POINT_2F& contentPoint) const {
  std::array<IUILayer*, 1> nextLayers {mTabViewUILayer.get()};
  const auto mapping = mHeaderUILayer->GetCoordinateMapping(
    nextLayers,
    {
      .mTabView = mCurrentTabView,
      .mKneeboardView = std::static_pointer_cast<IKneeboardView>(
        std::const_pointer_cast<KneeboardView>(this->shared_from_this())),
      .mIsActiveForInput = true,
    });

  const auto& contentArea = mapping.mContentArea;
  const D2D1_SIZE_F contentSize {
    contentArea.right - contentArea.left,
    contentArea.bottom - contentArea.top,
  };
  const auto& canvasSize = mapping.mCanvasSize;

  return {
    (contentPoint.x + contentArea.left) / canvasSize.width,
    (contentPoint.y + contentArea.top) / canvasSize.height,
  };
}

}// namespace OpenKneeboard
