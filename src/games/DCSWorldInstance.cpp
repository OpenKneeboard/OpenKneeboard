// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/DCSWorldInstance.hpp>
#include <OpenKneeboard/Game.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/utf8.hpp>

namespace OpenKneeboard {

nlohmann::json DCSWorldInstance::ToJson() const {
  auto j = GameInstance::ToJson();
  j["SavedGamesPath"] = this->mSavedGamesPath;
  return j;
}

static std::filesystem::path InferSavedGamesPath(
  const std::filesystem::path& executablePath) {
  const auto openBetaPath
    = DCSWorld::GetSavedGamesPath(DCSWorld::Version::OPEN_BETA);
  const bool haveOpenBetaPath = std::filesystem::is_directory(openBetaPath);

  if (
    haveOpenBetaPath
    && executablePath
      == DCSWorld::GetInstalledPath(DCSWorld::Version::OPEN_BETA)) {
    return openBetaPath;
  }

  const auto stablePath
    = DCSWorld::GetSavedGamesPath(DCSWorld::Version::STABLE);
  const auto haveStablePath = std::filesystem::is_directory(stablePath);

  if (
    haveStablePath
    && executablePath
      == DCSWorld::GetInstalledPath(DCSWorld::Version::STABLE)) {
    return stablePath;
  }

  if (haveOpenBetaPath) {
    return openBetaPath;
  }

  if (haveStablePath) {
    return stablePath;
  }

  return {};
}

DCSWorldInstance::DCSWorldInstance(
  const nlohmann::json& j,
  const std::shared_ptr<Game>& game)
  : GameInstance(j, game) {
  if (j.contains("SavedGamesPath")) {
    mSavedGamesPath = j.at("SavedGamesPath").get<std::filesystem::path>();
  } else {
    mSavedGamesPath = InferSavedGamesPath(mPathPattern);
  }
}

DCSWorldInstance::DCSWorldInstance(
  const std::string& name,
  const std::filesystem::path& path,
  const std::shared_ptr<Game>& game)
  : GameInstance(name, path, game) {
  mSavedGamesPath = InferSavedGamesPath(path);
}

}// namespace OpenKneeboard
