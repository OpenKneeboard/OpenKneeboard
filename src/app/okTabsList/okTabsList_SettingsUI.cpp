#include "okTabsList_SettingsUI.h"

#include "okTabsList_SharedState.h"

okTabsList::SettingsUI::SettingsUI(
  wxWindow* parent,
  const std::shared_ptr<okTabsList::SharedState>& state)
  : wxPanel(parent, wxID_ANY) {
  this->SetLabel(_("Tabs"));
}

okTabsList::SettingsUI::~SettingsUI() {
}
