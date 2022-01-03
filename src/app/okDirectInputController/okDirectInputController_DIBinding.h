#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "okDirectInputController.h"

#include <guiddef.h>

struct okDirectInputController::DIBinding {
  GUID InstanceGuid;
  std::string InstanceName;
  uint8_t ButtonIndex;
  wxEventTypeTag<wxCommandEvent> EventType;
};
