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

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/frame.h>

#include <memory>

class wxBookCtrlEvent;

class okMainWindow final : public wxFrame {
 public:
  okMainWindow();
  virtual ~okMainWindow();

 private:
  class Impl;
  std::unique_ptr<Impl> p;

  void OnExit(wxCommandEvent&);
  void OnGameEvent(wxThreadEvent&);
  void OnShowSettings(wxCommandEvent&);

  void OnPreviousTab(wxCommandEvent&);
  void OnNextTab(wxCommandEvent&);
  void OnPreviousPage(wxCommandEvent&);
  void OnNextPage(wxCommandEvent&);
  void OnToggleVisibility(wxCommandEvent&);

  void OnTabChanged(wxBookCtrlEvent&);

  void UpdateTabs();
  void UpdateSHM();
};
