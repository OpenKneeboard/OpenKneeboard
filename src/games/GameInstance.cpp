// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Game.hpp>
#include <OpenKneeboard/GameInstance.hpp>

#include <shims/winrt/base.h>

#include <OpenKneeboard/utf8.hpp>

namespace OpenKneeboard {

nlohmann::json GameInstance::ToJson() const {
  return {
    {"Name", this->mName},
    {"PathPattern", this->mPathPattern},
    {"Type", this->mGame->GetNameForConfigFile()},
    {"OverlayAPI", this->mOverlayAPI},
    {"LastSeenPath", this->mLastSeenPath},
  };
}

static uint64_t gNextInstanceID = 1;

GameInstance::GameInstance(
  const nlohmann::json& j,
  const std::shared_ptr<Game>& game)
  : mInstanceID(gNextInstanceID++) {
  mName = j.at("Name");
  mGame = game;

  if (j.contains("OverlayAPI")) {
    mOverlayAPI = j.at("OverlayAPI");
  }

  if (j.contains("PathPattern")) {
    mPathPattern = j.at("PathPattern");
  } else if (j.contains("Path")) {
    mPathPattern = j.at("Path");
    mLastSeenPath = j.at("Path").get<std::filesystem::path>();
  }

  if (j.contains("LastSeenPath")) {
    mLastSeenPath = j.at("LastSeenPath").get<std::filesystem::path>();
  }
}

GameInstance::GameInstance(
  const std::string& name,
  const std::filesystem::path& path,
  const std::shared_ptr<Game>& game)
  : mInstanceID(gNextInstanceID++),
    mName(name),
    mPathPattern(path.string()),
    mLastSeenPath(path),
    mGame(game) {
}

GameInstance::~GameInstance() = default;

}// namespace OpenKneeboard
