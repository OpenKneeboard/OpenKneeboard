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
