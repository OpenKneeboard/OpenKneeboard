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
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/WintabTablet.h>
#include <shims/wx.h>
#include <wx/frame.h>

#include <memory>

#include "Events.h"
#include "Settings.h"
#include "okOpenVRThread.h"
#include "okSHMRenderer.h"
#include "okTab.h"
#include "okTabsList.h"

class wxBookCtrlEvent;
class wxNotebook;

namespace OpenKneeboard {
class KneeboardState;
}

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
  void PostGameEvent(wxThreadEvent&);
  void OnShowSettings(wxCommandEvent&);

  void OnPreviousTab(wxCommandEvent&);
  void OnNextTab(wxCommandEvent&);
  void OnToggleVisibility(wxCommandEvent&);

  void OnTabChanged(wxBookCtrlEvent&);

  void UpdateTabs();

  OpenKneeboard::DXResources mDXResources;
  std::vector<okConfigurableComponent*> mConfigurables;
  wxNotebook* mNotebook = nullptr;
  Settings mSettings = Settings::Load();

  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;
  std::unique_ptr<okSHMRenderer> mSHMRenderer;
  std::unique_ptr<OpenKneeboard::WintabTablet> mTablet;
};
