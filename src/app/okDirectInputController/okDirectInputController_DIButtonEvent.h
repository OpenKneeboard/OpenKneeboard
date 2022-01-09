#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <dinput.h>

#include <cstdint>

#include "okDirectInputController.h"

wxDECLARE_EVENT(okEVT_DI_BUTTON, wxThreadEvent);

struct okDirectInputController::DIButtonEvent {
  bool valid = false;
  DIDEVICEINSTANCE instance;
  uint8_t buttonIndex;
  bool pressed;

  operator bool() const;
};
