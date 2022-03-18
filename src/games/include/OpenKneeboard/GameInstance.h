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

#include <OpenKneeboard/Game.h>

#include <memory>
#include <nlohmann/json.hpp>

namespace OpenKneeboard {

struct GameInstance {
  GameInstance() = delete;
  GameInstance(const GameInstance&) = delete;
  GameInstance(GameInstance&&) = delete;

  GameInstance(
    const nlohmann::json& json,
    const std::shared_ptr<Game>& game);
  GameInstance(
    const std::string& name,
    const std::filesystem::path& path,
    const std::shared_ptr<Game>& game);

  std::string mName;
  std::filesystem::path mPath;
  std::shared_ptr<Game> mGame;

  virtual nlohmann::json ToJson() const;
};
};// namespace OpenKneeboard
