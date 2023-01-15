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
#include <OpenKneeboard/dprint.h>
#include <shims/winrt/base.h>

#include <thread>
#include <unordered_map>
#include <unordered_set>

// clang-format off
#include <Windows.h>
#include <Dbt.h>
#include <Rpc.h>
#include <dinput.h>
// clang-format on

using namespace OpenKneeboard;

namespace OpenKneeboard {

std::weak_ptr<DirectInputAdapter> DirectInputAdapter::gInstance;

std::shared_ptr<DirectInputAdapter> DirectInputAdapter::Create(
  HWND hwnd,
  const DirectInputSettings& settings) {
  if (gInstance.lock()) {
    throw std::logic_error("Only one DirectInputAdapter at a time");
  }

  auto ret = shared_with_final_release<DirectInputAdapter>(
    new DirectInputAdapter(hwnd, settings));
  ret->Reload();
  gInstance = ret->weak_from_this();
  return ret;
}

DirectInputAdapter::DirectInputAdapter(
  HWND hwnd,
  const DirectInputSettings& settings)
  : mWindow(hwnd), mSettings(settings) {
  mPreviousWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
    hwnd,
    GWLP_WNDPROC,
    reinterpret_cast<LONG_PTR>(&DirectInputAdapter::WindowProc)));
  winrt::check_pointer(mPreviousWindowProc);
  winrt::check_hresult(DirectInput8Create(
    GetModuleHandle(nullptr),
    DIRECTINPUT_VERSION,
    IID_IDirectInput8,
    mDI8.put_void(),
    NULL));
}

void DirectInputAdapter::LoadSettings(const DirectInputSettings& settings) {
  mSettings = settings;
  this->Reload();
  this->evSettingsChangedEvent.Emit();
}

winrt::Windows::Foundation::IAsyncAction DirectInputAdapter::ReleaseDevices() {
  this->RemoveAllEventListeners();

  for (auto& [id, device]: mDevices) {
    device.mListener.Cancel();
  }
  for (auto& [id, device]: mDevices) {
    co_await winrt::resume_on_signal(device.mListenerCompletionHandle.get());
  }
  mDevices.clear();
}

winrt::fire_and_forget DirectInputAdapter::Reload() {
  const auto keepAlive = shared_from_this();

  co_await this->ReleaseDevices();

  for (auto diDeviceInstance: GetDirectInputDevices(mDI8.get())) {
    auto device = DirectInputDevice::Create(diDeviceInstance);
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

    winrt::handle completionHandle {CreateEvent(nullptr, TRUE, FALSE, nullptr)};

    mDevices.insert_or_assign(
      device->GetID(),
      DeviceState {
        .mDevice = device,
        .mListener
        = DirectInputListener::Run(mDI8, device, completionHandle.get()),
        .mListenerCompletionHandle = std::move(completionHandle),
      });

    AddEventListener(device->evUserActionEvent, this->evUserActionEvent);
    AddEventListener(
      device->evBindingsChangedEvent, this->evSettingsChangedEvent);
  }

  this->evAttachedControllersChangedEvent.Emit();
}

winrt::fire_and_forget DirectInputAdapter::final_release(
  std::unique_ptr<DirectInputAdapter> self) {
  co_await self->ReleaseDevices();
}

DirectInputAdapter::~DirectInputAdapter() {
  this->RemoveAllEventListeners();
  if (mPreviousWindowProc) {
    SetWindowLongPtrW(
      mWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(mPreviousWindowProc));
    mPreviousWindowProc = nullptr;
  }
}

std::vector<std::shared_ptr<UserInputDevice>> DirectInputAdapter::GetDevices()
  const {
  std::vector<std::shared_ptr<UserInputDevice>> devices;
  for (const auto& [id, device]: mDevices) {
    devices.push_back(
      std::static_pointer_cast<UserInputDevice>(device.mDevice));
  }
  return devices;
}

DirectInputSettings DirectInputAdapter::GetSettings() const {
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

LRESULT CALLBACK DirectInputAdapter::WindowProc(
  _In_ HWND hwnd,
  _In_ UINT uMsg,
  _In_ WPARAM wParam,
  _In_ LPARAM lParam) {
  auto instance = gInstance.lock();
  if (!instance) {
    OPENKNEEBOARD_BREAK;
    return 0;
  }

  if (uMsg == WM_DEVICECHANGE && wParam == DBT_DEVNODES_CHANGED) {
    dprint("Devices changed, reloading DirectInput device list");
    instance->Reload();
  }

  return CallWindowProc(
    instance->mPreviousWindowProc, hwnd, uMsg, wParam, lParam);
}

}// namespace OpenKneeboard
