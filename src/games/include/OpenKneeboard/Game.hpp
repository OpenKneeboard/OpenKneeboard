// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <shims/nlohmann/json_fwd.hpp>

#include <filesystem>
#include <memory>
#include <vector>

namespace OpenKneeboard {

struct GameInstance;

class Game : public std::enable_shared_from_this<Game> {
 public:
  Game();
  virtual ~Game();

  virtual bool MatchesPath(const std::filesystem::path&) const;

  virtual const char* GetNameForConfigFile() const = 0;
  virtual std::string GetUserFriendlyName(const std::filesystem::path&) const
    = 0;
  virtual std::vector<std::filesystem::path> GetInstalledPaths() const = 0;
  virtual bool DiscardOculusDepthInformationDefault() const;

  virtual std::shared_ptr<GameInstance> CreateGameInstance(
    const std::filesystem::path&);
  virtual std::shared_ptr<GameInstance> CreateGameInstance(
    const nlohmann::json&);
};
}// namespace OpenKneeboard
