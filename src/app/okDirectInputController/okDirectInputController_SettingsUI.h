#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <dinput.h>

#include "okDirectInputController.h"
#include "okDirectInputController_DIBinding.h"

class okDirectInputController::SettingsUI final : public wxPanel {
 public:
  SettingsUI(
    wxWindow* parent,
    const std::shared_ptr<SharedState>&);
  ~SettingsUI();

 private:
  struct DeviceButtons;
  std::vector<DIDEVICEINSTANCE> mDevices;
  std::shared_ptr<SharedState> mControllerState;
  std::vector<DeviceButtons> mDeviceButtons;

  wxButton* CreateBindButton(
    wxWindow* parent,
    int deviceIndex,
    const wxEventTypeTag<wxCommandEvent>& eventType);

  wxDialog* CreateBindInputDialog(bool haveExistingBinding);

  void OnBindButton(
    wxCommandEvent& ev,
    int deviceIndex,
    const wxEventTypeTag<wxCommandEvent>& eventType);
};
