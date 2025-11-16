// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Game.hpp>

namespace OpenKneeboard {

class GenericGame final : public Game {
 public:
  virtual bool MatchesPath(const std::filesystem::path&) const override;
  virtual const char* GetNameForConfigFile() const override;
  virtual std::string GetUserFriendlyName(
    const std::filesystem::path&) const override;
  virtual std::vector<std::filesystem::path> GetInstalledPaths() const override;
};

}// namespace OpenKneeboard
