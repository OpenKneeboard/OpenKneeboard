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
#include <OpenKneeboard/DirectInputAdapter.h>
#include <OpenKneeboard/DirectInputDevice.h>
#include <OpenKneeboard/DirectInputListener.h>
#include <OpenKneeboard/GetDirectInputDevices.h>
#include <OpenKneeboard/UserInputButtonBinding.h>
#include <Rpc.h>
#include <dinput.h>
#include <shims/winrt.h>

#include <nlohmann/json.hpp>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using namespace OpenKneeboard;

namespace {

struct JSONButtonBinding {
  std::unordered_set<uint64_t> Buttons;
  UserAction Action;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONButtonBinding, Buttons, Action);
struct JSONDevice {
  std::string ID;
  std::string Name;
  std::string Kind;
  std::vector<JSONButtonBinding> ButtonBindings;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONDevice, ID, Name, Kind, ButtonBindings);
struct JSONSettings {
  std::unordered_map<std::string, JSONDevice> Devices;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONSettings, Devices);

}// namespace

namespace OpenKneeboard {

DirectInputAdapter::DirectInputAdapter(const nlohmann::json& jsonSettings)
  : mInitialSettings(jsonSettings) {
  winrt::check_hresult(DirectInput8Create(
    GetModuleHandle(nullptr),
    DIRECTINPUT_VERSION,
    IID_IDirectInput8,
    mDI8.put_void(),
    NULL));

  JSONSettings settings;
  if (!jsonSettings.is_null()) {
    jsonSettings.get_to(settings);
  }

  for (auto diDeviceInstance: GetDirectInputDevices(mDI8.get())) {
    auto device = std::make_shared<DirectInputDevice>(diDeviceInstance);
    AddEventListener(device->evUserActionEvent, this->evUserActionEvent);
    AddEventListener(
      device->evBindingsChangedEvent, this->evSettingsChangedEvent);
    if (settings.Devices.contains(device->GetID())) {
      std::vector<UserInputButtonBinding> bindings;
      for (const auto& binding:
           settings.Devices.at(device->GetID()).ButtonBindings) {
        bindings.push_back({
          device,
          binding.Buttons,
          binding.Action,
        });
      }
      device->SetButtonBindings(bindings);
    }
    mDevices.push_back(device);
  }

  mDIThread = std::jthread([=](std::stop_token stopToken) {
    SetThreadDescription(GetCurrentThread(), L"DirectInput Thread");
    DirectInputListener listener(mDI8, mDevices);
    auto promise = listener.Run();
    std::stop_callback stopper(stopToken, [&promise]() {
      /** SPAMMMMMMMMMMMMMMM
       * MMMM */
      promise.Cancel();
    });
    try {
      promise.get();
    } catch (winrt::hresult_canceled) {
    }
  });
}

DirectInputAdapter::~DirectInputAdapter() {
  this->RemoveAllEventListeners();
  mDIThread.request_stop();
}

std::vector<std::shared_ptr<UserInputDevice>> DirectInputAdapter::GetDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> devices;
  for (const auto& device: mDevices) {
    devices.push_back(std::static_pointer_cast<UserInputDevice>(device));
  }
  return devices;
}

nlohmann::json DirectInputAdapter::GetSettings() const {
  JSONSettings settings;
  if (!mInitialSettings.is_null()) {
    mInitialSettings.get_to(settings);
  }

  for (const auto& device: mDevices) {
    const auto deviceID = device->GetID();
    if (settings.Devices.contains(deviceID)) {
      settings.Devices.erase(deviceID);
    }

    if (device->GetButtonBindings().empty()) {
      continue;
    }

    std::vector<JSONButtonBinding> buttonBindings;
    for (const auto& binding: device->GetButtonBindings()) {
      buttonBindings.push_back({
        .Buttons = binding.GetButtonIDs(),
        .Action = binding.GetAction(),
      });
    }

    const auto kind
      = (device->GetDIDeviceInstance().dwDevType & 0xff) == DI8DEVTYPE_KEYBOARD
      ? "Keyboard"
      : "GameController";

    settings.Devices[deviceID] = {
      .ID = deviceID,
      .Name = device->GetName(),
      .Kind = kind,
      .ButtonBindings = buttonBindings,
    };
  }
  return settings;
}

}// namespace OpenKneeboard
