// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/GameInstance.hpp>

namespace OpenKneeboard {

struct DCSWorldInstance final : public GameInstance {
  DCSWorldInstance() = delete;
  DCSWorldInstance(const DCSWorldInstance&) = delete;
  DCSWorldInstance(DCSWorldInstance&&) = delete;

  DCSWorldInstance(
    const nlohmann::json& json,
    const std::shared_ptr<Game>& game);
  DCSWorldInstance(
    const std::string& name,
    const std::filesystem::path& path,
    const std::shared_ptr<Game>& game);

  std::filesystem::path mSavedGamesPath;

  virtual nlohmann::json ToJson() const;
};

}// namespace OpenKneeboard
