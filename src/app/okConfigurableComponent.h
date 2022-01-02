#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <json.hpp>

/// Expected to raise okEVT_SETTINGS_CHANGED
class okConfigurableComponent : public wxEvtHandler {
 public:
  virtual wxWindow* GetSettingsUI(wxWindow* parent) = 0;
  virtual nlohmann::json GetSettings() const = 0;
};
