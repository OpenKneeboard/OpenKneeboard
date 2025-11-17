// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/Game.hpp>
#include <OpenKneeboard/GameInstance.hpp>
#include <OpenKneeboard/GamesList.hpp>
#include <OpenKneeboard/GenericGame.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/scope_exit.hpp>

#include <windows.h>

namespace OpenKneeboard {
GamesList::GamesList(KneeboardState* state, const nlohmann::json& config)
  : mKneeboardState(state),
    mGames({std::make_shared<DCSWorld>(), std::make_shared<GenericGame>()}) {
  LoadSettings(config);
}

void GamesList::OnGameChanged(
  DWORD processID,
  const std::filesystem::path& path,
  const std::shared_ptr<GameInstance>& game) {
  if (!(processID && (!path.empty()) && game)) {
    this->evGameChangedEvent.Emit(processID, game);
    return;
  }

  if (path != game->mLastSeenPath) {
    game->mLastSeenPath = path;
    this->evSettingsChangedEvent.Emit();
  }

  this->evGameChangedEvent.Emit(processID, game);
}

void GamesList::LoadDefaultSettings() {
  for (const auto& game: mGames) {
    for (const auto& path: game->GetInstalledPaths()) {
      mInstances.push_back(game->CreateGameInstance(path));
    }
  }
}

GamesList::~GamesList() {
  this->RemoveAllEventListeners();
}

nlohmann::json GamesList::GetSettings() const {
  auto games = nlohmann::json::array();

  for (const auto& game: mInstances) {
    games.push_back(game->ToJson());
  }
  return {{"Configured", games}};
}

void GamesList::LoadSettings(const nlohmann::json& config) {
  mInstances.clear();
  const scope_exit emitOnExit(
    [this]() { this->evSettingsChangedEvent.Emit(); });

  if (config.is_null()) {
    LoadDefaultSettings();
    return;
  }

  auto list = config.at("Configured");

  for (const auto& jsonInstance: list) {
    const std::string type = jsonInstance.at("Type");
    const auto game
      = std::ranges::find(mGames, type, &Game::GetNameForConfigFile);
    if (game == mGames.end()) {
      dprint.Error("Unsupported game type: `{}`", type);
      OPENKNEEBOARD_BREAK;
      continue;
    }

    auto instance = (*game)->CreateGameInstance(jsonInstance);
    const auto corrected = FixPathPattern(instance->mPathPattern);
    if (!corrected) {
      dprint.Warning(
        "Removing game `{}` - {}",
        instance->mPathPattern,
        magic_enum::enum_name(corrected.error()));
      continue;
    }
    if (corrected != instance->mPathPattern) {
      dprint.Warning(
        "Correcting game `{}` to `{}`",
        instance->mPathPattern,
        corrected.value());
      instance->mPathPattern = corrected.value();
      instance->mLastSeenPath = corrected.value();
      instance->mName
        = std::filesystem::path {corrected.value()}.stem().string();
    }
    mInstances.push_back(instance);
  }
}

std::vector<std::shared_ptr<Game>> GamesList::GetGames() const {
  return mGames;
}

std::vector<std::shared_ptr<GameInstance>> GamesList::GetGameInstances() const {
  return mInstances;
}

void GamesList::SetGameInstances(
  const std::vector<std::shared_ptr<GameInstance>>& instances) {
  mInstances = instances;
  this->evSettingsChangedEvent.Emit();
}

std::expected<std::string, GamesList::PathPatternError>
GamesList::FixPathPattern(const std::string_view pattern) {
  using namespace std::string_view_literals;

  constexpr std::array Launchers {
    std::tuple {
      "\\ui\\iRacingUI.exe"sv,
      "\\iRacingSim64DX11.exe"sv,
    },
    std::tuple {
      "\\EDLaunch.exe"sv,
      "\\Products\\elite-dangerous-odyssey-64\\EliteDangerous64.exe"sv,
    },
  };

  for (auto&& [launcher, game]: Launchers) {
    if (!pattern.ends_with(launcher)) {
      continue;
    }
    std::string_view base {pattern};
    base.remove_suffix(launcher.size());
    const auto ret = std::format("{}{}", base, game);
    if (std::filesystem::exists(ret)) {
      return ret;
    }
    return std::unexpected {PathPatternError::Launcher};
  }

  constexpr std::array CommonUtilities {
    "\\Content Manager.exe"sv,// 3rd-party Assetto Corsa launcher
    "\\Discord.exe"sv,
    "\\OpenKneeboardApp.exe"sv,// ?!
    "\\RacelabApps.exe"sv,
    "\\SimHubWPF.exe"sv,
    "\\Spotify.exe"sv,
    "\\StreamDeck.exe"sv,
    "\\chrome.exe"sv,
    "\\firefox.exe"sv,
    "\\iOverlay.exe"sv,
    "\\msedge.exe"sv,
    "\\opera.exe"sv,
  };

  for (auto&& it: CommonUtilities) {
    if (pattern.ends_with(it)) {
      return std::unexpected {PathPatternError::NotAGame};
    }
  }

  return std::string {pattern};
}

}// namespace OpenKneeboard
