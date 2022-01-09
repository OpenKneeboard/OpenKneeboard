#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "okDirectInputController.h"

#include <guiddef.h>

struct okDirectInputController::DIBinding {
  GUID instanceGuid;
  std::string instanceName;
  uint8_t buttonIndex;
  wxEventTypeTag<wxCommandEvent> eventType;
};
