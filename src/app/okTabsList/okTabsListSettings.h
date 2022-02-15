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

#include <shims/wx.h>

#include "okTabsList.h"

class wxListEvent;
class wxListView;

class okTabsListSettings final : public wxPanel {
 public:
  okTabsListSettings(
    wxWindow* parent,
    const OpenKneeboard::DXResources& dxr,
    const std::shared_ptr<OpenKneeboard::KneeboardState>& kneeboard);
  virtual ~okTabsListSettings();

  OpenKneeboard::Event<> evSettingsChangedEvent;

 private:
  OpenKneeboard::DXResources mDXR;
  std::shared_ptr<OpenKneeboard::KneeboardState> mKneeboard;
  wxListView* mList = nullptr;

  void OnAddTab(wxCommandEvent&);
  void OnRemoveTab(wxCommandEvent&);

  void OnMoveTabUp(wxCommandEvent&);
  void OnMoveTabDown(wxCommandEvent&);

  enum class Direction {
    UP,
    DOWN,
  };
  void MoveTab(Direction);

  void InsertTab(const std::shared_ptr<OpenKneeboard::Tab>&);
  void InsertFolderTab();
};
