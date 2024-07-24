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

#include <OpenKneeboard/DCSWorld.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ITab.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <shims/filesystem>

#include <OpenKneeboard/utf8.hpp>

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

  virtual winrt::fire_and_forget OnAPIEvent(
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
