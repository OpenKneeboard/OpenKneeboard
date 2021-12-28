#pragma once

#include "okPageController.h"

#include <memory>

// Using DirectInput instead of wxJoystick so we can use
// all 128 buttons, rather than just the first 32
class okDirectInputPageController final : public okPageController {
 public:
  okDirectInputPageController();
  virtual ~okDirectInputPageController();
  
  virtual wxString GetTitle() const override;
  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;
 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void OnDIButtonEvent(const wxThreadEvent&);
};
