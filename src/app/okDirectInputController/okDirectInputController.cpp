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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "okDirectInputController.h"

#include <Rpc.h>
#include <dinput.h>
#include <winrt/base.h>

#include "GetDirectInputDevices.h"
#include "okDirectInputController_DIBinding.h"
#include "okDirectInputController_DIButtonEvent.h"
#include "okDirectInputController_DIButtonListener.h"
#include "okDirectInputController_DIThread.h"
#include "okDirectInputController_SettingsUI.h"
#include "okDirectInputController_SharedState.h"
#include "okEvents.h"

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

struct okDirectInputController::State : public SharedState {
  std::unique_ptr<DIThread> directInputThread;
};

okDirectInputController::okDirectInputController(
  const nlohmann::json& jsonSettings)
  : p(std::make_shared<State>()) {
  DirectInput8Create(
    GetModuleHandle(nullptr),
    DIRECTINPUT_VERSION,
    IID_IDirectInput8,
    p->di8.put_void(),
    NULL);

  p->directInputThread = std::make_unique<DIThread>(this, p->di8);
  p->directInputThread->Run();

  this->Bind(okEVT_DI_BUTTON, &okDirectInputController::OnDIButtonEvent, this);

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

    std::optional<wxEventTypeTag<wxCommandEvent>> eventType;
#define ACTION(x) \
  if (binding.Action == #x) { \
    eventType = okEVT_##x; \
  }
    ACTION(PREVIOUS_TAB);
    ACTION(NEXT_TAB);
    ACTION(PREVIOUS_PAGE);
    ACTION(NEXT_PAGE);
#undef ACTION
    if (!eventType) {
      continue;
    }

    UUID uuid;
    UuidFromStringA(
      reinterpret_cast<RPC_CSTR>(const_cast<char*>(binding.Device.c_str())),
      &uuid);
    p->bindings.push_back({
      .instanceGuid = uuid,
      .instanceName = deviceName->second.InstanceName,
      .buttonIndex = binding.ButtonIndex,
      .eventType = *eventType,
    });
  }
}

okDirectInputController::~okDirectInputController() {
  p->directInputThread->Wait();
}

void okDirectInputController::OnDIButtonEvent(const wxThreadEvent& ev) {
  if (p->hook) {
    wxQueueEvent(p->hook, ev.Clone());
    return;
  }

  auto be = ev.GetPayload<DIButtonEvent>();
  if (!be.pressed) {
    // We act on keydown
    return;
  }

  for (auto& it: p->bindings) {
    if (it.instanceGuid != be.instance.guidInstance) {
      continue;
    }
    if (it.buttonIndex != be.buttonIndex) {
      continue;
    }
    wxQueueEvent(this, new wxCommandEvent(it.eventType));
  }
}

wxWindow* okDirectInputController::GetSettingsUI(wxWindow* parent) {
  auto ret = new okDirectInputController::SettingsUI(parent, p);
  ret->Bind(okEVT_SETTINGS_CHANGED, [this](auto& ev) {
    wxQueueEvent(this, ev.Clone());
  });
  return ret;
}

nlohmann::json okDirectInputController::GetSettings() const {
  JSONSettings settings;

  for (const auto& binding: p->bindings) {
    RPC_CSTR rpcUuid;
    UuidToStringA(&binding.instanceGuid, &rpcUuid);
    const std::string uuid(reinterpret_cast<char*>(rpcUuid));
    RpcStringFreeA(&rpcUuid);

    settings.Devices[uuid] = {binding.instanceName};
    std::string action;
#define ACTION(x) \
  if (binding.eventType == okEVT_##x) { \
    action = #x; \
  }
    ACTION(PREVIOUS_TAB);
    ACTION(NEXT_TAB);
    ACTION(PREVIOUS_PAGE);
    ACTION(NEXT_PAGE);
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
