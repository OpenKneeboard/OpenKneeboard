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

#include <nlohmann/json.hpp>

#include <memory>

namespace OpenKneeboard {

// This must match the values in the Game Settings UI
#define OPENKNEEBOARD_OVERLAY_APIS \
  IT(None) \
  IT(AutoDetect) \
  IT(SteamVR) \
  IT(OpenXR) \
  IT(OculusD3D11) \
  IT(OculusD3D12) \
  IT(NonVRD3D11)

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

  const uint64_t mInstanceID;

  std::string mName;
  std::string mPathPattern;
  std::filesystem::path mLastSeenPath;
  OverlayAPI mOverlayAPI {OverlayAPI::AutoDetect};
  std::shared_ptr<Game> mGame;

  virtual nlohmann::json ToJson() const;
};
};// namespace OpenKneeboard
