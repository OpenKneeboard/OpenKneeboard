// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/CursorEvent.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/KneeboardView.hpp>
#include <OpenKneeboard/OTDIPCClient.hpp>
#include <OpenKneeboard/TabletInputAdapter.hpp>
#include <OpenKneeboard/TabletInputDevice.hpp>
#include <OpenKneeboard/UserAction.hpp>
#include <OpenKneeboard/UserInputButtonBinding.hpp>
#include <OpenKneeboard/UserInputButtonEvent.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>

#include <shims/nlohmann/json.hpp>

#include <CommCtrl.h>
#include <KnownFolders.h>

#include <WinTrust.h>
#include <SoftPub.h>

#include <atomic>
#include <bit>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace OpenKneeboard {

static std::atomic_flag gHaveInstance;

std::shared_ptr<TabletInputAdapter> TabletInputAdapter::Create(
  HWND,
  KneeboardState* kbs,
  const TabletSettings& tablet) {
  if (gHaveInstance.test_and_set()) {
    throw std::logic_error("There can only be one TabletInputAdapter");
  }
  auto ret
    = std::shared_ptr<TabletInputAdapter>(new TabletInputAdapter(kbs, tablet));
  ret->Init();

  return ret;
}

TabletInputAdapter::TabletInputAdapter(
  KneeboardState* kneeboard,
  const TabletSettings& settings)
  : mKneeboard(kneeboard) {
  OPENKNEEBOARD_TraceLoggingScope("TabletInputAdapter::TabletInputAdapter()");
  LoadSettings(settings);
}

void TabletInputAdapter::Init() {
  this->StartOTDIPC();
}

bool TabletInputAdapter::HaveAnyTablet() const {
  if (!mOTDDevices.empty()) {
    return true;
  }
  return false;
}

void TabletInputAdapter::StartOTDIPC() {
  if (mOTDIPC) {
    return;
  }

  mOTDIPC = OTDIPCClient::Create();
  AddEventListener(
    mOTDIPC->evTabletInputEvent,
    std::bind_front(&TabletInputAdapter::OnOTDInput, this));
  AddEventListener(
    mOTDIPC->evDeviceInfoReceivedEvent,
    std::bind_front(&TabletInputAdapter::OnOTDDevice, this));
  for (const auto& device: mOTDIPC->GetTablets()) {
    this->GetOTDDevice(device.mDevicePersistentID);
  }
}

task<void> TabletInputAdapter::StopOTDIPC() {
  OPENKNEEBOARD_TraceLoggingCoro("TabletInputAdapter::StopOTDIPC()");
  if (mOTDIPC) {
    co_await mOTDIPC->DisposeAsync();
  }
  mOTDIPC.reset();
}

std::shared_ptr<TabletInputDevice> TabletInputAdapter::CreateDevice(
  const std::string& name,
  const std::string& id) {
  auto device = std::make_shared<TabletInputDevice>(
    name, id, TabletOrientation::RotateCW90);
  LoadSettings(mSettings, device);

  AddEventListener(
    device->evBindingsChangedEvent, this->evSettingsChangedEvent);
  AddEventListener(device->evOrientationChangedEvent, [this]() {
    this->evSettingsChangedEvent.Emit();
  });
  AddEventListener(device->evUserActionEvent, this->evUserActionEvent);

  return device;
}

void TabletInputAdapter::LoadSettings(const TabletSettings& settings) {
  if (settings == mSettings) {
    return;
  }
  mSettings = settings;

  for (auto& device: this->GetDevices()) {
    auto tabletDevice = std::static_pointer_cast<TabletInputDevice>(device);
    LoadSettings(settings, tabletDevice);
  }
  this->evSettingsChangedEvent.Emit();
}

void TabletInputAdapter::LoadSettings(
  const TabletSettings& settings,
  const std::shared_ptr<TabletInputDevice>& tablet) {
  const auto deviceID = tablet->GetID();
  if (!settings.mDevices.contains(deviceID)) {
    tablet->SetButtonBindings({});
    return;
  }

  auto& jsonDevice = settings.mDevices.at(deviceID);
  tablet->SetOrientation(jsonDevice.mOrientation);
  std::vector<UserInputButtonBinding> bindings;
  for (const auto& binding: jsonDevice.mExpressKeyBindings) {
    bindings.push_back({
      tablet,
      binding.mButtons,
      binding.mAction,
    });
  }
  tablet->SetButtonBindings(bindings);
}

task<void> TabletInputAdapter::DisposeAsync() noexcept {
  OPENKNEEBOARD_TraceLoggingCoro("TabletInputAdapter::DisposeAsync()");
  const auto disposing = co_await mDisposal.StartOnce();
  if (!disposing) {
    co_return;
  }
  co_await this->StopOTDIPC();
}

TabletInputAdapter::~TabletInputAdapter() {
  OPENKNEEBOARD_TraceLoggingScope("TabletInputAdapter::~TabletInputAdapter()");
  this->RemoveAllEventListeners();
  gHaveInstance.clear();
}

std::vector<std::shared_ptr<UserInputDevice>> TabletInputAdapter::GetDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> ret;

  for (const auto& [_, device]: mOTDDevices) {
    ret.push_back(device);
  }

  return ret;
}

void TabletInputAdapter::OnTabletInput(
  const TabletInfo& tablet,
  const TabletState& state,
  const std::shared_ptr<TabletInputDevice>& device) {
  auto& auxButtons = mAuxButtons[tablet.mDevicePersistentID];

  if (state.mAuxButtons != auxButtons) {
    const uint32_t changedMask = state.mAuxButtons ^ auxButtons;
    const bool isPressed = state.mAuxButtons & changedMask;
    const uint64_t buttonIndex = std::countr_zero(changedMask);
    auxButtons = state.mAuxButtons;

    device->evButtonEvent.Emit({
      device,
      buttonIndex,
      isPressed,
    });
    return;
  }

  const auto view = mKneeboard->GetActiveViewForGlobalInput();

  auto maxX = tablet.mMaxX;
  auto maxY = tablet.mMaxY;

  if (state.mIsActive) {
    float x {}, y {};
    switch (device->GetOrientation()) {
      case TabletOrientation::Normal:
        x = state.mX;
        y = state.mY;
        break;
      case TabletOrientation::RotateCW90:
        x = maxY - state.mY;
        y = state.mX;
        std::swap(maxX, maxY);
        break;
      case TabletOrientation::RotateCW180:
        x = maxX - state.mX;
        y = maxY - state.mY;
        break;
      case TabletOrientation::RotateCW270:
        x = state.mY;
        y = maxX - state.mX;
        std::swap(maxX, maxY);
        break;
    }

    // Cursor events use 0..1 in canvas coordinates, so we need
    // to adapt for the aspect ratio

    // 1. scale to canvas size
    auto canvasSize = view->GetPreferredSize().mPixelSize;

    const auto scaleX = canvasSize.mWidth / maxX;
    const auto scaleY = canvasSize.mHeight / maxY;
    // in most cases, we use `std::min` - that would be for fitting the tablet
    // in the canvas bounds, but we want to fit the canvas in the tablet, so
    // doing the opposite
    const auto scale = std::max(scaleX, scaleY);

    x *= scale;
    y *= scale;

    // 2. get back to 0..1
    x /= canvasSize.mWidth;
    y /= canvasSize.mHeight;

    CursorEvent event {
      .mTouchState = (state.mPenButtons & 1) ? CursorTouchState::TouchingSurface
                                             : CursorTouchState::NearSurface,
      .mX = std::clamp<float>(x, 0, 1),
      .mY = std::clamp<float>(y, 0, 1),
      .mPressure = static_cast<float>(state.mPressure) / tablet.mMaxPressure,
      .mButtons = state.mPenButtons,
    };

    view->PostCursorEvent(event);
  } else {
    view->PostCursorEvent({});
  }
}

TabletSettings TabletInputAdapter::GetSettings() const {
  auto settings = mSettings;

  for (auto& device: this->GetDevices()) {
    auto tablet = std::static_pointer_cast<TabletInputDevice>(device);
    const auto id = device->GetID();
    auto& deviceSettings = settings.mDevices[id];
    deviceSettings = {
      .mID = id,
      .mName = device->GetName(),
      .mExpressKeyBindings = {},
      .mOrientation = tablet->GetOrientation(),
    };

    for (const auto& binding: device->GetButtonBindings()) {
      deviceSettings.mExpressKeyBindings.push_back({
        .mButtons = binding.GetButtonIDs(),
        .mAction = binding.GetAction(),
      });
    }
  }

  return settings;
}

std::shared_ptr<TabletInputDevice> TabletInputAdapter::GetOTDDevice(
  const std::string& id) {
  auto it = mOTDDevices.find(id);
  if (it != mOTDDevices.end()) {
    return it->second;
  }

  auto info = mOTDIPC->GetTablet(id);
  if (!info) {
    return nullptr;
  }

  auto device = CreateDevice(info->mDeviceName, info->mDevicePersistentID);
  mOTDDevices[info->mDevicePersistentID] = device;
  evDeviceConnectedEvent.Emit(device);
  return device;
}

OpenKneeboard::fire_and_forget TabletInputAdapter::OnOTDDevice(
  TabletInfo tablet) {
  if (!mSettings.mWarnIfOTDIPCUnusuable) {
    mSettings.mWarnIfOTDIPCUnusuable = true;
    this->evSettingsChangedEvent.Emit();
  }
  auto weak = weak_from_this();
  co_await mUIThread;
  if (auto self = weak.lock()) {
    self->GetOTDDevice(tablet.mDevicePersistentID);
  }
}

OpenKneeboard::fire_and_forget TabletInputAdapter::OnOTDInput(
  std::string id,
  TabletState state) {
  auto weak = weak_from_this();
  co_await mUIThread;
  auto self = weak.lock();
  if (!self) {
    co_return;
  }
  auto tablet = mOTDIPC->GetTablet(id);
  if (!tablet) {
    dprint("Received OTD input without device info");
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  auto device = GetOTDDevice(id);
  if (!device) {
    dprint("Received OTD input but couldn't create a TabletInputDevice");
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  this->OnTabletInput(*tablet, state, device);
}

std::vector<TabletInfo> TabletInputAdapter::GetTabletInfo() const {
  if (mOTDIPC) {
    return mOTDIPC->GetTablets();
  }
  return {};
}

}// namespace OpenKneeboard
