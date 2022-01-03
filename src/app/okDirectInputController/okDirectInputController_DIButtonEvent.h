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
  bool Valid = false;
  DIDEVICEINSTANCE Instance;
  uint8_t ButtonIndex;
  bool Pressed;

  operator bool() const;
};
