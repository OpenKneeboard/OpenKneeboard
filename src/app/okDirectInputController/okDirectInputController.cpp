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
#include "okDirectInputController.h"

#include <Rpc.h>
#include <dinput.h>
#include <shims/winrt.h>

#include <nlohmann/json.hpp>
#include <thread>

#include "GetDirectInputDevices.h"
#include "okDirectInputController_DIBinding.h"
#include "okDirectInputController_DIButtonEvent.h"
#include "okDirectInputController_DIButtonListener.h"
#include "okDirectInputSettings.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Rpcrt4.lib")

using namespace OpenKneeboard;

namespace {
struct JSONBinding final {
  std::string Device;
  uint8_t ButtonIndex;
  std::string Action;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONBinding, Device, ButtonIndex, Action);

struct JSONDevice {
  std::string InstanceName;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONDevice, InstanceName);

struct JSONSettings {
  std::map<std::string, JSONDevice> Devices;
  std::vector<JSONBinding> Bindings;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JSONSettings, Devices, Bindings);
}// namespace

okDirectInputController::okDirectInputController(
  const nlohmann::json& jsonSettings) {
  winrt::check_hresult(DirectInput8Create(
    GetModuleHandle(nullptr),
    DIRECTINPUT_VERSION,
    IID_IDirectInput8,
    mDI8.put_void(),
    NULL));

  mDIThread = std::jthread([=](std::stop_token stopToken) {
    DIButtonListener listener(mDI8, GetDirectInputDevices(mDI8.get()));
    this->AddEventListener(
      listener.evButtonEvent, &okDirectInputController::OnDIButtonEvent, this);
    listener.Run(stopToken);
  });

  JSONSettings settings;
  try {
    jsonSettings.get_to(settings);
  } catch (nlohmann::json::exception&) {
    return;
  }

  for (const auto& binding: settings.Bindings) {
    const auto& deviceName = settings.Devices.find(binding.Device);
    if (deviceName == settings.Devices.end()) {
      continue;
    }

    std::optional<UserAction> action;
#define ACTION(x) \
  if (binding.Action == #x) { \
    action = UserAction::x; \
  }
    ACTION(PREVIOUS_TAB);
    ACTION(NEXT_TAB);
    ACTION(PREVIOUS_PAGE);
    ACTION(NEXT_PAGE);
    ACTION(TOGGLE_VISIBILITY);
#undef ACTION
    if (!action) {
      continue;
    }

    UUID uuid;
    UuidFromStringA(
      reinterpret_cast<RPC_CSTR>(const_cast<char*>(binding.Device.c_str())),
      &uuid);
    mBindings.push_back({
      .instanceGuid = uuid,
      .instanceName = deviceName->second.InstanceName,
      .buttonIndex = binding.ButtonIndex,
      .action = *action,
    });
  }
}

okDirectInputController::~okDirectInputController() {
}

void okDirectInputController::OnDIButtonEvent(const DIButtonEvent& be) {
  if (mHook) {
    mHook(be);
    return;
  }

  if (!be.pressed) {
    // We act on keydown
    return;
  }

  for (auto& it: mBindings) {
    if (it.instanceGuid != be.instance.guidInstance) {
      continue;
    }
    if (it.buttonIndex != be.buttonIndex) {
      continue;
    }
    evUserAction(it.action);
  }
}

IDirectInput8W* okDirectInputController::GetDirectInput() const {
  return mDI8.get();
}

std::vector<okDirectInputController::DIBinding>
okDirectInputController::GetBindings() const {
  return mBindings;
}

void okDirectInputController::SetBindings(
  const std::vector<DIBinding>& bindings) {
  mBindings = bindings;
}

void okDirectInputController::SetHook(
  std::function<void(const DIButtonEvent&)> hook) {
  mHook = hook;
}

nlohmann::json okDirectInputController::GetSettings() const {
  JSONSettings settings;

  for (const auto& binding: mBindings) {
    RPC_CSTR rpcUuid;
    UuidToStringA(&binding.instanceGuid, &rpcUuid);
    const std::string uuid(reinterpret_cast<char*>(rpcUuid));
    RpcStringFreeA(&rpcUuid);

    settings.Devices[uuid] = {binding.instanceName};
    std::string action;
#define ACTION(x) \
  if (binding.action == UserAction::x) { \
    action = #x; \
  }
    ACTION(PREVIOUS_TAB);
    ACTION(NEXT_TAB);
    ACTION(PREVIOUS_PAGE);
    ACTION(NEXT_PAGE);
    ACTION(TOGGLE_VISIBILITY);
#undef ACTION
    if (action.empty()) {
      continue;
    }

    settings.Bindings.push_back({
      .Device = uuid,
      .ButtonIndex = binding.buttonIndex,
      .Action = action,
    });
  }
  return settings;
}
