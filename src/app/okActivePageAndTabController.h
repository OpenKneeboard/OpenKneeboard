#pragma once

#include "okConfigurableComponent.h"

/** Controller to change what tab and page are active.
 *
 * Expected to raise events:
 * - okEVT_SETTINGS_CHANGED
 * - okEVT_{PREVIOUS,NEXT}_{PAGE,TAB}
 */
class okActivePageAndTabController : public okConfigurableComponent {
 public:
  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;
};
