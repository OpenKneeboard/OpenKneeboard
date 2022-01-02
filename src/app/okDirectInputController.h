#pragma once

#include <memory>

#include "okActivePageAndTabController.h"

// Using DirectInput instead of wxJoystick so we can use
// all 128 buttons, rather than just the first 32
class okDirectInputController final : public okActivePageAndTabController {
 public:
  okDirectInputController() = delete;
  okDirectInputController(const nlohmann::json& settings);
  virtual ~okDirectInputController();

  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;

 private:
  class Impl;
  std::shared_ptr<Impl> p;

  void OnDIButtonEvent(const wxThreadEvent&);
};
