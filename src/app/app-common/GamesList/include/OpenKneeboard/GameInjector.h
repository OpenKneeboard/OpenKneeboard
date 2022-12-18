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
#include <OpenKneeboard/bitflags.h>
#include <Windows.h>

#include <memory>
#include <mutex>
#include <shims/filesystem>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenKneeboard {

struct GameInstance;

enum class InjectedDlls : uint32_t {
  None = 0,
  TabletProxy = 1,
  AutoDetection = 1 << 1,
  NonVRD3D11 = 1 << 2,
  OculusD3D11 = 1 << 3,
  OculusD3D12 = 1 << 4,
};

template <>
constexpr bool is_bitflags_v<InjectedDlls> = true;

class GameInjector final {
 public:
  GameInjector();
  bool Run(std::stop_token);

  Event<DWORD, std::shared_ptr<GameInstance>> evGameChangedEvent;
  void SetGameInstances(const std::vector<std::shared_ptr<GameInstance>>&);

  static bool AlreadyInjected(HANDLE process, const std::filesystem::path& dll);
  static bool InjectDll(HANDLE process, const std::filesystem::path& dll);

 private:
  void CheckProcess(DWORD processID, std::wstring_view exeBaseName);
  std::vector<std::shared_ptr<GameInstance>> mGames;
  std::mutex mGamesMutex;

  std::filesystem::path mTabletProxyDll;
  std::filesystem::path mOverlayAutoDetectDll;
  std::filesystem::path mOverlayNonVRD3D11Dll;
  std::filesystem::path mOverlayOculusD3D11Dll;
  std::filesystem::path mOverlayOculusD3D12Dll;

  std::unordered_map<DWORD, InjectedDlls> mProcessCache;
};

}// namespace OpenKneeboard
