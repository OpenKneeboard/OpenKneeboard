#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <guiddef.h>

#include "okDirectInputController.h"

struct okDirectInputController::DIBinding {
  GUID instanceGuid;
  std::string instanceName;
  uint8_t buttonIndex;
  wxEventTypeTag<wxCommandEvent> eventType;
};
