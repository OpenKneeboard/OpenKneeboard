#pragma once

#include <dinput.h>
#include <winrt/base.h>

#include "okDirectInputController.h"

class okDirectInputController::DIButtonListener final : public wxEvtHandler {
 public:
  DIButtonListener(
    const winrt::com_ptr<IDirectInput8>& di,
    const std::vector<DIDEVICEINSTANCE>& instances);
  ~DIButtonListener();
  void Cancel();

  DIButtonEvent Poll();

 private:
  struct DeviceInfo;
  std::vector<DeviceInfo> mDevices;
  HANDLE mCancelHandle = INVALID_HANDLE_VALUE;
};
