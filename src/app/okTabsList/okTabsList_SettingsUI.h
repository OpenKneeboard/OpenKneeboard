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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include "okTabsList.h"

class wxListEvent;
class wxListView;

class okTabsList::SettingsUI final : public wxPanel {
 public:
  SettingsUI(wxWindow* parent, const std::shared_ptr<SharedState>&);
  virtual ~SettingsUI();

 private:
  std::shared_ptr<SharedState> s;
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
