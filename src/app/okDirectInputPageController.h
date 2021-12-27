#pragma once

#include "okPageController.h"

#include <memory>

// Using DirectInput instead of wxJoystick so we can use
// all 128 buttons, rather than just the first 32
class okDirectInputPageController final : public okPageController {
 public:
  virtual wxString GetTitle() const override;
  virtual wxWindow* GetSettingsUI(wxWindow* parent) const override;
};
