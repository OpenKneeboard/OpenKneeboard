#include "okActivePageAndTabController.h"

okActivePageAndTabController::okActivePageAndTabController() {
}

okActivePageAndTabController::~okActivePageAndTabController() {
}

wxWindow* okActivePageAndTabController::GetSettingsUI(wxWindow* parent) {
  return nullptr;
}

nlohmann::json okActivePageAndTabController::GetSettings() const {
  return {};
}
