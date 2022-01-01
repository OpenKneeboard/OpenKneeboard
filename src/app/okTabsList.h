#pragma once

#include "okConfigurableComponent.h"

#include <OpenKneeboard/Tab.h>

class okTabsList final : public okConfigurableComponent {
  private:
    std::vector<std::shared_ptr<OpenKneeboard::Tab>> mTabs;
  public:
    okTabsList() = delete;
    okTabsList(const nlohmann::json& config);

    std::vector<std::shared_ptr<OpenKneeboard::Tab>> GetTabs() const;

    virtual ~okTabsList();
    virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
    virtual nlohmann::json GetSettings() const override;
};
