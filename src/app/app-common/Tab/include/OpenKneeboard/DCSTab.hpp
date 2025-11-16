// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <filesystem>

namespace OpenKneeboard {

struct APIEvent;

class DCSTab : public virtual ITab, public virtual EventReceiver {
 public:
  DCSTab(KneeboardState*);
  virtual ~DCSTab();

  DCSTab() = delete;

 protected:
  static constexpr std::string_view DebugInformationHeader = _(
    "A tick or a cross indicates whether or not the folder exists, not whether "
    "or not it is meant to exist. Some crosses are expected, and not "
    "necessarily an error.\n");

  virtual OpenKneeboard::fire_and_forget OnAPIEvent(
    APIEvent,
    std::filesystem::path installPath,
    std::filesystem::path savedGamesPath)
    = 0;

  std::filesystem::path ToAbsolutePath(const std::filesystem::path&);

 private:
  std::filesystem::path mInstallPath;
  std::filesystem::path mSavedGamesPath;
  EventHandlerToken mAPIEventToken;

  void OnAPIEvent(const APIEvent&);
};

}// namespace OpenKneeboard
