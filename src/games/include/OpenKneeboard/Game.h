/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <filesystem>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <vector>

namespace OpenKneeboard {

struct GameInstance;

class Game : public std::enable_shared_from_this<Game> {
 public:
  Game();
  virtual ~Game();

  virtual bool MatchesPath(const std::filesystem::path&) const;

  virtual const char* GetNameForConfigFile() const = 0;
  virtual std::string GetUserFriendlyName(
    const std::filesystem::path&) const = 0;
  virtual std::vector<std::filesystem::path> GetInstalledPaths() const = 0;
  virtual bool DiscardOculusDepthInformationDefault() const;

	virtual std::shared_ptr<GameInstance> CreateGameInstance(
		const std::filesystem::path&);
	virtual std::shared_ptr<GameInstance> CreateGameInstance(
		const nlohmann::json&);

};
}// namespace OpenKneeboard
