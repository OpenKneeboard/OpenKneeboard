#pragma once

#include "okActivePageAndTabController.h"

#include <memory>

// Using DirectInput instead of wxJoystick so we can use
// all 128 buttons, rather than just the first 32
class okDirectInputController final : public okActivePageAndTabController {
 public:
  okDirectInputController();
  virtual ~okDirectInputController();
  
  virtual wxString GetTitle() const override;
  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;
 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void OnDIButtonEvent(const wxThreadEvent&);
};
