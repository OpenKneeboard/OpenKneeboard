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

#include <OpenKneeboard/Events.h>
#include <Windows.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

namespace OpenKneeboard {

struct GameInstance;

class GameInjector final {
 public:
  GameInjector();
  bool Run(std::stop_token);

  Event<std::shared_ptr<GameInstance>> evGameChanged;
  void SetGameInstances(const std::vector<std::shared_ptr<GameInstance>>&);

 private:
  void CheckProcess(DWORD processID, std::wstring_view exeBaseName);
  std::vector<std::shared_ptr<GameInstance>> mGames;
  std::mutex mGamesMutex;

  std::filesystem::path mMarkerDll;
  std::filesystem::path mTabletProxyDll;
  std::filesystem::path mOverlayAutoDetectDll;
  std::filesystem::path mOverlayNonVRD3D11Dll;
  std::filesystem::path mOverlayOculusD3D11Dll;
  std::filesystem::path mOverlayOculusD3D12Dll;
};

}// namespace OpenKneeboard
