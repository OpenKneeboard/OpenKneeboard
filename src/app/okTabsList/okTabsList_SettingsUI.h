#pragma once

#include "okTabsList.h"

class okTabsList::SettingsUI final : public wxPanel {
  public:
    SettingsUI(wxWindow* parent, const std::shared_ptr<okTabsList::SharedState>&);
    virtual ~SettingsUI();
};
