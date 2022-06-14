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
#include <OpenKneeboard/DirectInputAdapter.h>
#include <OpenKneeboard/GameEventServer.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/InterprocessRenderer.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/OpenVRKneeboard.h>
#include <OpenKneeboard/OpenXRMode.h>
#include <OpenKneeboard/Tab.h>
#include <OpenKneeboard/TabState.h>
#include <OpenKneeboard/TabWithGameEvents.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/UserAction.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>

namespace OpenKneeboard {

KneeboardState::KneeboardState(HWND hwnd, const DXResources& dxr)
  : mDXResources(dxr) {
  AddEventListener(evCursorEvent, &KneeboardState::OnCursorEvent, this);

  if (!mSettings.NonVR.is_null()) {
    mFlatConfig = mSettings.NonVR;
  }
  if (!mSettings.NonVR.is_null()) {
    mVRConfig = mSettings.VR;
  }
  if (!mSettings.App.is_null()) {
    mAppSettings = mSettings.App;
  }
  if (!mSettings.Doodle.is_null()) {
    mDoodleSettings = mSettings.Doodle;
  }

  mGamesList = std::make_unique<GamesList>(mSettings.Games);
  AddEventListener(
    mGamesList->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);
  mTabsList = std::make_unique<TabsList>(dxr, this, mSettings.Tabs);
  mInterprocessRenderer
    = std::make_unique<InterprocessRenderer>(mDXResources, this);

  mTabletInput
    = std::make_unique<TabletInputAdapter>(hwnd, this, mSettings.TabletInput);
  AddEventListener(
    mTabletInput->evUserActionEvent, &KneeboardState::OnUserAction, this);
  AddEventListener(
    mTabletInput->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);

  mDirectInput = std::make_unique<DirectInputAdapter>(mSettings.DirectInputV2);
  AddEventListener(
    mDirectInput->evUserActionEvent, &KneeboardState::OnUserAction, this);
  AddEventListener(
    mDirectInput->evSettingsChangedEvent, &KneeboardState::SaveSettings, this);

  mGameEventThread = std::jthread([this](std::stop_token stopToken) {
    SetThreadDescription(GetCurrentThread(), L"GameEvent Thread");
    GameEventServer server;
    this->AddEventListener(
      server.evGameEvent, &KneeboardState::OnGameEvent, this);
    server.Run(stopToken);
  });

  if (mVRConfig.mEnableSteamVR) {
    this->StartOpenVRThread();
  }

  UpdateLayout();
}

KneeboardState::~KneeboardState() {
}

std::vector<std::shared_ptr<TabState>> KneeboardState::GetTabs() const {
  return mTabs;
}

void KneeboardState::SetTabs(
  const std::vector<std::shared_ptr<TabState>>& tabs) {
  if (std::ranges::equal(tabs, mTabs)) {
    return;
  }
  auto oldTab = mCurrentTab;

  for (const auto& tab: tabs) {
    if (std::find(mTabs.begin(), mTabs.end(), tab) != mTabs.end()) {
      continue;
    }
    AddEventListener(tab->evNeedsRepaintEvent, [=]() {
      if (tab == this->GetCurrentTab()) {
        this->evNeedsRepaintEvent.Emit();
      }
    });
    AddEventListener(tab->evAvailableFeaturesChangedEvent, [=]() {
      if (tab == this->GetCurrentTab()) {
        this->evNeedsRepaintEvent.Emit();
      }
    });
    AddEventListener(
      tab->evPageChangedEvent, &KneeboardState::UpdateLayout, this);
  }

  mTabs = tabs;
  SaveSettings();

  auto it = std::find(tabs.begin(), tabs.end(), mCurrentTab);
  if (it == tabs.end()) {
    mCurrentTab = tabs.empty() ? nullptr : tabs.front();
  }
  UpdateLayout();
  evTabsChangedEvent.Emit();
}

void KneeboardState::InsertTab(
  uint8_t index,
  const std::shared_ptr<TabState>& tab) {
  auto tabs = mTabs;
  tabs.insert(tabs.begin() + index, tab);
  SetTabs(tabs);
}

void KneeboardState::AppendTab(const std::shared_ptr<TabState>& tab) {
  auto tabs = mTabs;
  tabs.push_back(tab);
  SetTabs(tabs);
}

void KneeboardState::RemoveTab(uint8_t index) {
  if (index >= mTabs.size()) {
    return;
  }

  auto tabs = mTabs;
  tabs.erase(tabs.begin() + index);
  SetTabs(tabs);
}

uint8_t KneeboardState::GetTabIndex() const {
  auto it = std::find(mTabs.begin(), mTabs.end(), mCurrentTab);
  if (it == mTabs.end()) {
    return 0;
  }
  return static_cast<uint8_t>(it - mTabs.begin());
}

void KneeboardState::SetTabIndex(uint8_t index) {
  if (index >= mTabs.size()) {
    return;
  }
  if (mCurrentTab == mTabs.at(index)) {
    return;
  }
  if (mCurrentTab) {
    mCurrentTab->PostCursorEvent({});
  }
  mCurrentTab = mTabs.at(index);
  UpdateLayout();
  evCurrentTabChangedEvent.Emit(index);
}

void KneeboardState::SetTabID(uint64_t id) {
  if (mCurrentTab && mCurrentTab->GetInstanceID() == id) {
    return;
  }
  for (uint8_t i = 0; i < mTabs.size(); ++i) {
    if (mTabs.at(i)->GetInstanceID() == id) {
      SetTabIndex(i);
      return;
    }
  }
}

void KneeboardState::PreviousTab() {
  const auto current = GetTabIndex();
  if (current > 0) {
    SetTabIndex(current - 1);
  }
}

void KneeboardState::NextTab() {
  const auto current = GetTabIndex();
  const auto count = GetTabs().size();
  if (current + 1 < count) {
    SetTabIndex(current + 1);
  }
}

std::shared_ptr<TabState> KneeboardState::GetCurrentTab() const {
  return mCurrentTab;
}

void KneeboardState::NextPage() {
  if (mCurrentTab) {
    mCurrentTab->NextPage();
    UpdateLayout();
  }
}

void KneeboardState::PreviousPage() {
  if (mCurrentTab) {
    mCurrentTab->PreviousPage();
    UpdateLayout();
  }
}

void KneeboardState::UpdateLayout() {
  const auto totalHeightRatio = 1 + (HeaderPercent / 100.0f);

  mContentNativeSize = {768, 1024};
  auto tab = GetCurrentTab();
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

  evNeedsRepaintEvent.Emit();
}

const D2D1_SIZE_U& KneeboardState::GetCanvasSize() const {
  return mCanvasSize;
}

const D2D1_SIZE_U& KneeboardState::GetContentNativeSize() const {
  return mContentNativeSize;
}

const D2D1_RECT_F& KneeboardState::GetHeaderRenderRect() const {
  return mHeaderRenderRect;
}

const D2D1_RECT_F& KneeboardState::GetContentRenderRect() const {
  return mContentRenderRect;
}

void KneeboardState::OnCursorEvent(const CursorEvent& ev) {
  mCursorPoint = {ev.mX, ev.mY};
  mHaveCursor = ev.mTouchState != CursorTouchState::NOT_NEAR_SURFACE;

  if (mCurrentTab) {
    mCurrentTab->PostCursorEvent(ev);
  }
}

void KneeboardState::OnUserAction(UserAction action) {
  switch (action) {
    case UserAction::TOGGLE_VISIBILITY:
      if (mInterprocessRenderer) {
        mInterprocessRenderer.reset();
      } else {
        mInterprocessRenderer
          = std::make_unique<InterprocessRenderer>(mDXResources, this);
      }
      return;
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
    case UserAction::TOGGLE_FORCE_ZOOM: {
      auto& flags = this->mVRConfig.mFlags;
      if (flags & VRRenderConfig::Flags::FORCE_ZOOM) {
        flags &= ~VRRenderConfig::Flags::FORCE_ZOOM;
      } else {
        flags |= VRRenderConfig::Flags::FORCE_ZOOM;
      }
      this->SaveSettings();
      this->evNeedsRepaintEvent.Emit();
      return;
    }
    case UserAction::RECENTER_VR:
      this->mVRConfig.mRecenterCount++;
      this->evNeedsRepaintEvent.Emit();
      return;
  }
  OPENKNEEBOARD_BREAK;
}

void KneeboardState::OnGameEvent(const GameEvent& ev) {
  if (ev.name == GameEvent::EVT_REMOTE_USER_ACTION) {
#define IT(ACTION) \
  if (ev.value == #ACTION) { \
    OnUserAction(UserAction::ACTION); \
    return; \
  }
    OPENKNEEBOARD_USER_ACTIONS
#undef IT
  }
  for (auto tab: mTabs) {
    auto receiver
      = std::dynamic_pointer_cast<TabWithGameEvents>(tab->GetRootTab());
    if (receiver) {
      receiver->PostGameEvent(ev);
    }
  }
}

bool KneeboardState::HaveCursor() const {
  return mHaveCursor;
}

D2D1_POINT_2F KneeboardState::GetCursorPoint() const {
  return mCursorPoint;
}

D2D1_POINT_2F KneeboardState::GetCursorCanvasPoint() const {
  return this->GetCursorCanvasPoint(this->GetCursorPoint());
}

D2D1_POINT_2F KneeboardState::GetCursorCanvasPoint(
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

std::vector<std::shared_ptr<UserInputDevice>> KneeboardState::GetInputDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> devices;
  for (const auto& subDevices:
       {mTabletInput->GetDevices(), mDirectInput->GetDevices()}) {
    devices.reserve(devices.size() + subDevices.size());
    for (const auto& device: subDevices) {
      devices.push_back(device);
    }
  }
  return devices;
}

FlatConfig KneeboardState::GetFlatConfig() const {
  return mFlatConfig;
}

void KneeboardState::SetFlatConfig(const FlatConfig& value) {
  mFlatConfig = value;
  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

VRConfig KneeboardState::GetVRConfig() const {
  return mVRConfig;
}

void KneeboardState::SetVRConfig(const VRConfig& value) {
  if (value.mEnableSteamVR != mVRConfig.mEnableSteamVR) {
    if (!value.mEnableSteamVR) {
      mOpenVRThread.request_stop();
    } else {
      this->StartOpenVRThread();
    }
  }

  if (value.mOpenXRMode != mVRConfig.mOpenXRMode) {
    SetOpenXRModeWithHelperProcess(value.mOpenXRMode, {mVRConfig.mOpenXRMode});
  }

  mVRConfig = value;
  this->SaveSettings();
  this->evNeedsRepaintEvent.Emit();
}

AppSettings KneeboardState::GetAppSettings() const {
  return mAppSettings;
}

void KneeboardState::SetAppSettings(const AppSettings& value) {
  mAppSettings = value;
  this->SaveSettings();
}

GamesList* KneeboardState::GetGamesList() const {
  return mGamesList.get();
}

void KneeboardState::SaveSettings() {
  if (mGamesList) {
    mSettings.Games = mGamesList->GetSettings();
  }
  if (mTabsList) {
    mSettings.Tabs = mTabsList->GetSettings();
  }
  if (mTabletInput) {
    mSettings.TabletInput = mTabletInput->GetSettings();
  }
  if (mDirectInput) {
    mSettings.DirectInputV2 = mDirectInput->GetSettings();
  }

  mSettings.NonVR = mFlatConfig;
  mSettings.VR = mVRConfig;
  mSettings.App = mAppSettings;
  mSettings.Doodle = mDoodleSettings;

  mSettings.Save();
}

DoodleSettings KneeboardState::GetDoodleSettings() {
  return mDoodleSettings;
}

void KneeboardState::SetDoodleSettings(const DoodleSettings& value) {
  mDoodleSettings = value;
  this->SaveSettings();
}

void KneeboardState::StartOpenVRThread() {
  mOpenVRThread = std::jthread([](std::stop_token stopToken) {
    SetThreadDescription(GetCurrentThread(), L"OpenVR Thread");
    OpenVRKneeboard().Run(stopToken);
  });
}

}// namespace OpenKneeboard
