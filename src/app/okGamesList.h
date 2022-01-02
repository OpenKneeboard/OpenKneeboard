#pragma once

#include "okConfigurableComponent.h"

#include <OpenKneeboard/Game.h>

#include "GameInstance.h"

class okGamesList final : public okConfigurableComponent {
  private:
    class Settings;
    std::vector<std::shared_ptr<OpenKneeboard::Game>> mGames;
    std::vector<OpenKneeboard::GameInstance> mInstances;
  public:
    okGamesList() = delete;
    okGamesList(const nlohmann::json& config);
    virtual ~okGamesList();
    virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
    virtual nlohmann::json GetSettings() const override;
};
