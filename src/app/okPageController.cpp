#include "okPageController.h"

okPageController::okPageController() {
}

okPageController::~okPageController() {
}

wxWindow* okPageController::GetSettingsUI(wxWindow* parent) {
  return nullptr;
}

nlohmann::json okPageController::GetSettings() const {
  return {};
}
