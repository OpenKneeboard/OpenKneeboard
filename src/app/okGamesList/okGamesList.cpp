#include "okGamesList.h"

#include "GenericGame.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "okEvents.h"
#include "okGameInjectorThread.h"
#include "okGamesList_SettingsUI.h"

using namespace OpenKneeboard;

okGamesList::okGamesList(const nlohmann::json& config) {
  mGames
    = {std::make_shared<Games::DCSWorld>(), std::make_shared<GenericGame>()};

  if (config.is_null()) {
    LoadDefaultSettings();
  } else {
    LoadSettings(config);
  }

  mInjector = new okGameInjectorThread(this, mInstances);
  mInjector->Run();
}

void okGamesList::LoadDefaultSettings() {
  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back({
        .Name = game->GetUserFriendlyName(path).ToStdString(),
        .Path = path,
        .Game = game,
      });
    }
  }
}

okGamesList::~okGamesList() {
}

wxWindow* okGamesList::GetSettingsUI(wxWindow* parent) {
  auto ret = new SettingsUI(parent, this);
  ret->Bind(okEVT_SETTINGS_CHANGED, [=](auto& ev) {
    mInjector->SetGameInstances(mInstances);
    wxQueueEvent(this, ev.Clone());
  });
  return ret;
}

nlohmann::json okGamesList::GetSettings() const {
  nlohmann::json games {};
  for (const auto& game: mInstances) {
    games.push_back(game.ToJson());
  }
  return {{"Configured", games}};
}

void okGamesList::LoadSettings(const nlohmann::json& config) {
  auto list = config.at("Configured");

  for (const auto& game: list) {
    mInstances.push_back(GameInstance::FromJson(game, mGames));
  }
}
