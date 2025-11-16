// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

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
#include <OpenKneeboard/WintabTablet.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/handles.hpp>

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

UINT_PTR TabletInputAdapter::gNextSubclassID = 1;

std::shared_ptr<TabletInputAdapter> TabletInputAdapter::Create(
  HWND hwnd,
  KneeboardState* kbs,
  const TabletSettings& tablet) {
  if (gHaveInstance.test_and_set()) {
    throw std::logic_error("There can only be one TabletInputAdapter");
  }
  auto ret = std::shared_ptr<TabletInputAdapter>(
    new TabletInputAdapter(hwnd, kbs, tablet));
  ret->Init();

  return ret;
}

TabletInputAdapter::TabletInputAdapter(
  HWND window,
  KneeboardState* kneeboard,
  const TabletSettings& settings)
  : mKneeboard(kneeboard), mWindow(window) {
  OPENKNEEBOARD_TraceLoggingScope("TabletInputAdapter::TabletInputAdapter()");
  LoadSettings(settings);
}

void TabletInputAdapter::Init() {
  this->StartWintab();
  this->StartOTDIPC();
}

bool TabletInputAdapter::HaveAnyTablet() const {
  if (!mOTDDevices.empty()) {
    return true;
  }
  if (mWintabDevice) {
    return true;
  }
  return false;
}

bool TabletInputAdapter::IsOTDIPCEnabled() const {
  return mSettings.mOTDIPC;
}

task<void> TabletInputAdapter::SetIsOTDIPCEnabled(bool value) {
  if (value == IsOTDIPCEnabled()) {
    co_return;
  }

  if (!mSettings.mWarnIfOTDIPCUnusuable) {
    mSettings.mWarnIfOTDIPCUnusuable = true;
  }

  mSettings.mOTDIPC = value;
  if (value) {
    StartOTDIPC();
  } else {
    co_await StopOTDIPC();
  }
  evSettingsChangedEvent.Emit();
}

void TabletInputAdapter::StartOTDIPC() {
  if (mOTDIPC) {
    return;
  }
  if (!mSettings.mOTDIPC) {
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
    this->GetOTDDevice(device.mDeviceID);
  }
}

task<void> TabletInputAdapter::StopOTDIPC() {
  OPENKNEEBOARD_TraceLoggingCoro("TabletInputAdapter::StopOTDIPC()");
  if (mOTDIPC) {
    co_await mOTDIPC->DisposeAsync();
  }
  mOTDIPC.reset();
}

WintabMode TabletInputAdapter::GetWintabMode() const {
  return mSettings.mWintab;
}

task<void> TabletInputAdapter::SetWintabMode(WintabMode mode) {
  auto weak = weak_from_this();

  if (mode == GetWintabMode()) {
    co_return;
  }

  if (mode == WintabMode::Disabled) {
    mSettings.mWintab = mode;
    StopWintab();
    evSettingsChangedEvent.Emit();
    co_return;
  }

  const auto availability = this->GetWinTabAvailability();
  if (availability != WinTabAvailability::Available) {
    switch (availability) {
      case WinTabAvailability::NotInstalled:
        dprint("WinTab: not installed");
        break;
      case WinTabAvailability::Skipping_OpenTabletDriverEnabled:
        dprint("WinTab: skipping, OpenTabletDriver enabled");
        break;
      case WinTabAvailability::Skipping_NoTrustedSignature:
        dprint("WinTab: skipping, unsigned");
        break;
      case WinTabAvailability::Available:
        std::unreachable();// make clang-tidy happy
    }
    co_return;
  }

  // Check that we can actually load Wintab before we save it; some drivers
  // - especially XP-Pen - will crash as soon as they're loaded
  if (!mWintabTablet) {
    dprint("Attempting to load wintab");
    unique_hmodule wintab {LoadLibraryW(L"WINTAB32.dll")};
    co_await winrt::resume_after(std::chrono::milliseconds(100));
    dprint("Loaded wintab!");
  }

  auto self = weak.lock();
  if (!self) {
    co_return;
  }

  mSettings.mWintab = mode;

  self.reset();
  co_await mUIThread;
  self = weak.lock();
  if (!self) {
    co_return;
  }

  try {
    StartWintab();
  } catch (const winrt::hresult_error& e) {
    dprint("Failed to initialize wintab: {}", winrt::to_string(e.message()));
    co_return;
  } catch (const std::exception& e) {
    dprint("Failed to initialize wintab: {}", e.what());
    co_return;
  }
  if (!mWintabTablet) {
    dprint("Initialized wintab, but no tablet attached");
  } else {
    auto priority = (mSettings.mWintab == WintabMode::Enabled)
      ? WintabTablet::Priority::AlwaysActive
      : WintabTablet::Priority::ForegroundOnly;
    mWintabTablet->SetPriority(priority);
    // Again, make sure that doesn't crash :)

    self.reset();
    co_await winrt::resume_after(std::chrono::milliseconds(100));
    self = weak.lock();
    if (!self) {
      co_return;
    }
  }
  evSettingsChangedEvent.Emit();
}

void TabletInputAdapter::StartWintab() {
  GetWinTabAvailability();
  if (mSettings.mWintab == WintabMode::Disabled) {
    return;
  }

  if (!mWintabTablet) {
    // If mode is 'Invasive', we manage background access by
    // injecting a DLL, so as far as our wintab is concerned,
    // it's only dealing with the foreground
    auto priority = (mSettings.mWintab == WintabMode::Enabled)
      ? WintabTablet::Priority::AlwaysActive
      : WintabTablet::Priority::ForegroundOnly;
    mWintabTablet = std::make_unique<WintabTablet>(mWindow, priority);
    if (!mWintabTablet->IsValid()) {
      mWintabTablet.reset();
      return;
    }
  }

  if (mSubclassID) {
    return;
  }
  mSubclassID = gNextSubclassID++;
  winrt::check_bool(SetWindowSubclass(
    mWindow,
    &TabletInputAdapter::SubclassProc,
    mSubclassID,
    reinterpret_cast<DWORD_PTR>(this)));

  auto info = mWintabTablet->GetDeviceInfo();
  mWintabDevice = this->CreateDevice(info.mDeviceName, info.mDeviceID);
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
  StopWintab();
  this->RemoveAllEventListeners();
  gHaveInstance.clear();
}

void TabletInputAdapter::StopWintab() {
  if (!mWintabTablet) {
    return;
  }

  if (!mSubclassID) {
    return;
  }

  RemoveWindowSubclass(mWindow, &TabletInputAdapter::SubclassProc, mSubclassID);
  mWintabTablet = {};
}

LRESULT TabletInputAdapter::SubclassProc(
  HWND hwnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam,
  UINT_PTR uIdSubclass,
  DWORD_PTR dwRefData) {
  auto instance = reinterpret_cast<TabletInputAdapter*>(dwRefData);

  instance->OnWintabMessage(uMsg, wParam, lParam);
  return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

std::vector<std::shared_ptr<UserInputDevice>> TabletInputAdapter::GetDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> ret;

  if (mWintabDevice) {
    ret.push_back(std::static_pointer_cast<UserInputDevice>(mWintabDevice));
  }

  for (const auto& [_, device]: mOTDDevices) {
    ret.push_back(device);
  }

  return ret;
}

void TabletInputAdapter::OnWintabMessage(
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam) {
  if (!mWintabTablet->ProcessMessage(uMsg, wParam, lParam)) {
    return;
  }
  const auto tablet = mWintabTablet->GetDeviceInfo();
  const auto state = mWintabTablet->GetState();
  this->OnTabletInput(tablet, state, mWintabDevice);
}

void TabletInputAdapter::OnTabletInput(
  const TabletInfo& tablet,
  const TabletState& state,
  const std::shared_ptr<TabletInputDevice>& device) {
  auto& auxButtons = mAuxButtons[tablet.mDeviceID];

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

  auto device = CreateDevice(info->mDeviceName, info->mDeviceID);
  mOTDDevices[info->mDeviceID] = device;
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
    self->GetOTDDevice(tablet.mDeviceID);
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

WinTabAvailability TabletInputAdapter::GetWinTabAvailability() {
  if (mSettings.mOTDIPC) {
    return WinTabAvailability::Skipping_OpenTabletDriverEnabled;
  }

  const auto path
    = Filesystem::GetKnownFolderPath<FOLDERID_System>() / "wintab32.dll";
  if (!std::filesystem::exists(path)) {
    return WinTabAvailability::NotInstalled;
  }

  const auto pathStr = path.wstring();

  WINTRUST_FILE_INFO fileInfo {
    .cbStruct = sizeof(WINTRUST_FILE_INFO),
    .pcwszFilePath = pathStr.c_str(),
  };
  WINTRUST_DATA wintrustData {
    .cbStruct = sizeof(WINTRUST_DATA),
    .dwUIChoice = WTD_UI_NONE,
    .fdwRevocationChecks = WTD_REVOCATION_CHECK_NONE,
    .dwUnionChoice = WTD_CHOICE_FILE,
    .pFile = &fileInfo,
    .dwStateAction = WTD_STATEACTION_VERIFY,
  };

  GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  if (
    WinVerifyTrust(
      static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &wintrustData)
    != 0) {
    return WinTabAvailability::Skipping_NoTrustedSignature;
  }

  return WinTabAvailability::Available;
}

std::vector<TabletInfo> TabletInputAdapter::GetTabletInfo() const {
  if (mOTDIPC) {
    return mOTDIPC->GetTablets();
  }
  if (mWintabTablet) {
    return {mWintabTablet->GetDeviceInfo()};
  }
  return {};
}

}// namespace OpenKneeboard
