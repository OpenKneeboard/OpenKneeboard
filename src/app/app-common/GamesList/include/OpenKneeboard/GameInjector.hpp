// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/bitflags.hpp>

#include <Windows.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OpenKneeboard {

struct GameInstance;

enum class InjectedDlls : uint32_t {
  None = 0,
  // TabletProxy = 1,
  // AutoDetection = 1 << 1,
  NonVRD3D11 = 1 << 2,
  OculusD3D11 = 1 << 3,
  OculusD3D12 = 1 << 4,
};

template <>
constexpr bool is_bitflags_v<InjectedDlls> = true;

class KneeboardState;

class GameInjector final : public EventReceiver,
                           public std::enable_shared_from_this<GameInjector> {
 public:
  static std::shared_ptr<GameInjector> Create(KneeboardState* kneeboardState);
  ~GameInjector();
  task<void> Run(std::stop_token);

  Event<
    DWORD,
    const std::filesystem::path&,
    const std::shared_ptr<GameInstance>&>
    evGameChangedEvent;
  void SetGameInstances(const std::vector<std::shared_ptr<GameInstance>>&);

  static bool InjectDll(HANDLE process, const std::filesystem::path& dll);

  GameInjector() = delete;

 private:
  GameInjector(KneeboardState* kneeboardState);
  static std::unordered_set<std::filesystem::path> GetProcessCurrentDLLs(
    HANDLE process);
  void CheckProcess(DWORD processID, std::wstring_view exeBaseName);
  KneeboardState* mKneeboardState {nullptr};
  std::vector<std::shared_ptr<GameInstance>> mGames;
  std::mutex mGamesMutex;

  std::filesystem::path mOverlayNonVRD3D11Dll;
  std::filesystem::path mOverlayOculusD3D11Dll;

  struct ProcessCacheEntry {
    enum class InjectionAccessState {
      NotTried,
      HaveInjectionAccess,
      Failed,
    };
    winrt::handle mHandle {};
    std::filesystem::path mPath;
    InjectionAccessState mInjectionAccessState {InjectionAccessState::NotTried};
    InjectedDlls mInjectedDlls {InjectedDlls::None};
    bool mHaveLoggedDLLs {false};
    uint32_t mCheckProcessCount = 0;
  };
  std::unordered_map<DWORD, ProcessCacheEntry> mProcessCache;
};

}// namespace OpenKneeboard
