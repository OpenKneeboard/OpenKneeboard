// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/Game.hpp>

#include <shims/nlohmann/json.hpp>

#include <memory>

namespace OpenKneeboard {

// This must match the values in the Game Settings UI
// 2024-12-05: Oculus+D3D12 support has been removed.
// Should remove it here too in the future and handle settings migration if
// there's not a compelling reason to bring it back.
#define OPENKNEEBOARD_OVERLAY_APIS \
  IT(None) \
  IT(AutoDetect) /* TODO: remove later */ \
  IT(SteamVR) \
  IT(OpenXR) \
  IT(OculusD3D11) /* TODO: remove later */ \
  IT(OculusD3D12) /* TODO: remove later */ \
  IT(NonVRD3D11) /* TODO: remove later */

enum class OverlayAPI {
#define IT(x) x,
  OPENKNEEBOARD_OVERLAY_APIS
#undef IT
};

#define IT(x) {OverlayAPI::x, #x},
NLOHMANN_JSON_SERIALIZE_ENUM(OverlayAPI, {OPENKNEEBOARD_OVERLAY_APIS});
#undef IT

struct GameInstance {
  GameInstance() = delete;
  GameInstance(const GameInstance&) = delete;
  GameInstance(GameInstance&&) = delete;

  GameInstance(const nlohmann::json& json, const std::shared_ptr<Game>& game);
  GameInstance(
    const std::string& name,
    const std::filesystem::path& path,
    const std::shared_ptr<Game>& game);

  virtual ~GameInstance();

  const uint64_t mInstanceID;

  std::string mName;
  std::string mPathPattern;
  std::filesystem::path mLastSeenPath;
  OverlayAPI mOverlayAPI {OverlayAPI::AutoDetect};
  std::shared_ptr<Game> mGame;

  virtual nlohmann::json ToJson() const;
};
};// namespace OpenKneeboard
