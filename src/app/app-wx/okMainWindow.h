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

#include <OpenKneeboard/CursorEvent.h>
#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/Settings.h>
#include <shims/wx.h>
#include <wx/frame.h>

#include <memory>
#include <thread>

#include "okTab.h"

class wxBookCtrlEvent;
class wxNotebook;

namespace OpenKneeboard {
enum class UserAction;
struct GameEvent;
class DirectInputAdapter;
class GamesList;
class KneeboardState;
class TabsList;
}// namespace OpenKneeboard

class okMainWindow final : public wxFrame,
                           private OpenKneeboard::EventReceiver {
 public:
  okMainWindow();
  virtual ~okMainWindow();

 private:
  void OnExit(wxCommandEvent&);
  void PostGameEvent(const OpenKneeboard::GameEvent&);
  void OnShowSettings(wxCommandEvent&);

  void OnToggleVisibility();

  void OnNotebookTabChanged(wxBookCtrlEvent&);
  void OnTabChanged(uint8_t tabIndex);

  void UpdateTabs();

  OpenKneeboard::DXResources mDXR;
  wxNotebook* mNotebook = nullptr;
  wxTimer mFrameTimer;
  OpenKneeboard::Settings mSettings = OpenKneeboard::Settings::Load();

  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;

  void InitUI();
};
