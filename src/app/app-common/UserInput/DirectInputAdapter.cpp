// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/DirectInputAdapter.hpp>
#include <OpenKneeboard/DirectInputDevice.hpp>
#include <OpenKneeboard/DirectInputListener.hpp>
#include <OpenKneeboard/GetDirectInputDevices.hpp>
#include <OpenKneeboard/UserInputButtonBinding.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>
#include <CommCtrl.h>
#include <Dbt.h>
#include <Rpc.h>
#include <dinput.h>

#include <thread>
#include <unordered_map>
#include <unordered_set>

using namespace OpenKneeboard;

namespace OpenKneeboard {

UINT_PTR DirectInputAdapter::gNextID = 0;

std::shared_ptr<DirectInputAdapter> DirectInputAdapter::Create(
  HWND hwnd,
  const DirectInputSettings& settings) {
  auto ret = shared_with_final_release<DirectInputAdapter>(
    new DirectInputAdapter(hwnd, settings));
  ret->Reload();
  return ret;
}

DirectInputAdapter::DirectInputAdapter(
  HWND hwnd,
  const DirectInputSettings& settings)
  : mWindow(hwnd), mSettings(settings) {
  mID = gNextID++;
  SetWindowSubclass(
    hwnd,
    &DirectInputAdapter::SubclassProc,
    mID,
    reinterpret_cast<DWORD_PTR>(this));

  winrt::check_hresult(DirectInput8Create(
    GetModuleHandle(nullptr),
    DIRECTINPUT_VERSION,
    IID_IDirectInput8,
    mDI8.put_void(),
    NULL));
}

void DirectInputAdapter::LoadSettings(const DirectInputSettings& settings) {
  if (mSettings == settings) {
    return;
  }

  mSettings = settings;
  this->Reload();
  this->evSettingsChangedEvent.Emit();
}

task<void> DirectInputAdapter::ReleaseDevices() {
  this->RemoveAllEventListeners();
  dprint("DirectInputAdapter::ReleaseDevices()");

  auto devices = [this]() {
    std::unique_lock lock(mDevicesMutex);
    return std::move(mDevices);
  }();
  mDevices.clear();

  dprint("Requesting directinput listener stops");
  for (auto&& [id, device]: devices) {
    device.mStop.request_stop();
  }
  dprint("Waiting for listeners to stop");
  for (auto&& [id, device]: devices) {
    co_await std::move(device.mListener);
  }
}

OpenKneeboard::fire_and_forget DirectInputAdapter::Reload() {
  const auto keepAlive = shared_from_this();

  co_await this->ReleaseDevices();

  this->UpdateDevices();
}

OpenKneeboard::fire_and_forget DirectInputAdapter::UpdateDevices() {
  // Make sure the lock is released first
  EventDelay delay;
  std::unique_lock lock(mDevicesMutex);

  if (mShuttingDown) {
    OPENKNEEBOARD_BREAK;
    co_return;
  }

  auto instances
    = GetDirectInputDevices(mDI8.get(), mSettings.mEnableMouseButtonBindings);

  std::vector<task<void>> stoppingListeners;

  for (auto dit = mDevices.begin(); dit != mDevices.end(); /* no increment */) {
    auto& device = dit->second.mDevice;
    const winrt::guid guid {device->GetID()};

    const auto iit
      // not a coroutine
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
      = std::ranges::find_if(instances, [guid](const auto& instance) {
          return guid == winrt::guid {instance.guidInstance};
        });
    if (iit != instances.end()) {
      ++dit;
      continue;
    }

    dprint(
      L"DirectInput device removed: {} ('{}')",
      winrt::to_hstring(guid),
      winrt::to_hstring(device->GetName()));
    dit->second.mStop.request_stop();
    stoppingListeners.push_back(std::move(dit->second.mListener));
    dit = mDevices.erase(dit);
  }

  for (const auto& instance: instances) {
    const auto id = winrt::to_string(winrt::to_hstring(instance.guidInstance));
    if (mDevices.contains(id)) {
      continue;
    }

    dprint(
      "DirectInput device added: {} ('{}')",
      winrt::to_string(instance.tszInstanceName),
      id);

    auto device = DirectInputDevice::Create(instance);
    assert(device->GetID() == id);

    if (mSettings.mDevices.contains(device->GetID())) {
      std::vector<UserInputButtonBinding> bindings;
      for (const auto& binding:
           mSettings.mDevices.at(device->GetID()).mButtonBindings) {
        bindings.push_back({
          device,
          binding.mButtons,
          binding.mAction,
        });
      }
      device->SetButtonBindings(bindings);
    }

    std::stop_source stopper;
    auto stopToken = stopper.get_token();

    mDevices.try_emplace(
      id,
      DeviceState {
        .mDevice = device,
        .mListener = DirectInputListener::Run(stopToken, mDI8, device),
        .mStop = stopper,
      });

    AddEventListener(device->evUserActionEvent, this->evUserActionEvent);
    AddEventListener(
      device->evBindingsChangedEvent, this->evSettingsChangedEvent);
  }

  lock.unlock();

  if (!stoppingListeners.empty()) {
    dprint("Waiting for DirectInput listeners to stop...");
    for (auto&& listener: stoppingListeners) {
      co_await std::move(listener);
    }
    dprint("DirectInput listeners stopped.");
  }

  this->evAttachedControllersChangedEvent.Emit();
}

OpenKneeboard::fire_and_forget DirectInputAdapter::final_release(
  std::unique_ptr<DirectInputAdapter> self) {
  self->mShuttingDown = true;
  co_await self->ReleaseDevices();
}

DirectInputAdapter::~DirectInputAdapter() {
  this->RemoveAllEventListeners();
  RemoveWindowSubclass(mWindow, &DirectInputAdapter::SubclassProc, mID);
}

std::vector<std::shared_ptr<UserInputDevice>> DirectInputAdapter::GetDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> devices;
  std::shared_lock lock(mDevicesMutex);
  for (const auto& [id, device]: mDevices) {
    devices.push_back(
      std::static_pointer_cast<UserInputDevice>(device.mDevice));
  }
  return devices;
}

DirectInputSettings DirectInputAdapter::GetSettings() const {
  std::shared_lock lock(mDevicesMutex);
  for (const auto& [deviceID, state]: mDevices) {
    const auto& device = state.mDevice;
    const auto kind
      = (device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD
      ? "Keyboard"
      : "GameController";

    auto& deviceSettings = mSettings.mDevices[deviceID];
    deviceSettings = {
      .mID = deviceID,
      .mName = device->GetName(),
      .mKind = kind,
      .mButtonBindings = {},
    };

    if (device->GetButtonBindings().empty()) {
      continue;
    }

    for (const auto& binding: device->GetButtonBindings()) {
      deviceSettings.mButtonBindings.push_back({
        .mButtons = binding.GetButtonIDs(),
        .mAction = binding.GetAction(),
      });
    }
  }

  return mSettings;
}

LRESULT DirectInputAdapter::SubclassProc(
  HWND hWnd,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam,
  UINT_PTR /*uIdSubclass*/,
  DWORD_PTR dwRefData) {
  auto instance = reinterpret_cast<DirectInputAdapter*>(dwRefData);
  if (uMsg == WM_DEVICECHANGE && wParam == DBT_DEVNODES_CHANGED) {
    dprint("Devices changed, updating DirectInput device list");
    instance->UpdateDevices();
  }

  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

}// namespace OpenKneeboard
