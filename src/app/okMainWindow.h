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
#include <OpenKneeboard/WintabTablet.h>
#include <shims/wx.h>
#include <wx/frame.h>

#include <memory>
#include <thread>

#include "Events.h"
#include "Settings.h"
#include "okSHMRenderer.h"
#include "okTab.h"

class wxBookCtrlEvent;
class wxNotebook;

namespace OpenKneeboard {
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

 protected:
  virtual WXLRESULT
  MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;

 private:
  void OnExit(wxCommandEvent&);
  void PostGameEvent(const OpenKneeboard::GameEvent&);
  void OnShowSettings(wxCommandEvent&);

  void OnToggleVisibility();

  void OnTabChanged(wxBookCtrlEvent&);

  void UpdateTabs();

  OpenKneeboard::DXResources mDXR;
  wxNotebook* mNotebook = nullptr;
  Settings mSettings = Settings::Load();

  std::unique_ptr<OpenKneeboard::DirectInputAdapter> mDirectInput;
  std::unique_ptr<OpenKneeboard::GamesList> mGamesList;
  std::unique_ptr<OpenKneeboard::TabsList> mTabsList;

  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;
  std::unique_ptr<okSHMRenderer> mSHMRenderer;
  std::unique_ptr<OpenKneeboard::WintabTablet> mTablet;

  std::vector<OpenKneeboard::CursorEvent> mBufferedCursorEvents;
  void FlushCursorEvents();
  wxTimer mCursorEventTimer;

  std::jthread mGameEventThread;
  std::jthread mOpenVRThread;

  void InitUI();
};
