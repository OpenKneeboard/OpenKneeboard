#include "okDirectInputController_DIButtonListener.h"

#include "okDirectInputController_DIButtonEvent.h"

struct okDirectInputController::DIButtonListener::DeviceInfo {
  DIDEVICEINSTANCE Instance;
  winrt::com_ptr<IDirectInputDevice8> Device;
  DIJOYSTATE2 State = {};
  HANDLE EventHandle = INVALID_HANDLE_VALUE;
};

okDirectInputController::DIButtonListener::DIButtonListener(
  const winrt::com_ptr<IDirectInput8>& di,
  const std::vector<DIDEVICEINSTANCE>& instances) {
  for (const auto& it: instances) {
    winrt::com_ptr<IDirectInputDevice8> device;
    di->CreateDevice(it.guidInstance, device.put(), NULL);
    if (!device) {
      continue;
    }

    HANDLE event {CreateEvent(nullptr, false, false, nullptr)};
    if (!event) {
      continue;
    }

    device->SetEventNotification(event);
    device->SetDataFormat(&c_dfDIJoystick2);
    device->SetCooperativeLevel(
      static_cast<wxApp*>(wxApp::GetInstance())->GetTopWindow()->GetHandle(),
      DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    device->Acquire();

    DIJOYSTATE2 state;
    device->GetDeviceState(sizeof(state), &state);

    mDevices.push_back({it, device, state, event});
  }
  mCancelHandle = CreateEvent(nullptr, false, false, nullptr);
}

okDirectInputController::DIButtonListener::~DIButtonListener() {
  CloseHandle(mCancelHandle);
}

void okDirectInputController::DIButtonListener::Cancel() {
  SetEvent(mCancelHandle);
}

okDirectInputController::DIButtonEvent
okDirectInputController::DIButtonListener::Poll() {
  std::vector<HANDLE> handles;
  for (const auto& it: mDevices) {
    handles.push_back(it.EventHandle);
  }
  handles.push_back(mCancelHandle);

  auto result
    = WaitForMultipleObjects(handles.size(), handles.data(), false, 100);
  auto idx = result - WAIT_OBJECT_0;
  if (idx <= 0 || idx >= (handles.size() - 1)) {
    return {};
  }

  auto& device = mDevices.at(idx);
  auto oldState = device.State;
  DIJOYSTATE2 newState;
  device.Device->Poll();
  device.Device->GetDeviceState(sizeof(newState), &newState);
  for (uint8_t i = 0; i < 128; ++i) {
    if (oldState.rgbButtons[i] != newState.rgbButtons[i]) {
      device.State = newState;
      return {
        true,
        device.Instance,
        static_cast<uint8_t>(i),
        (bool)(newState.rgbButtons[i] & (1 << 7))};
    }
  }
  return {};
}
