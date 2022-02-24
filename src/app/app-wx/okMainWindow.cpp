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
#include "okMainWindow.h"

#include <OpenKneeboard/DirectInputAdapter.h>
#include <OpenKneeboard/GamesList.h>
#include <OpenKneeboard/TabletInputAdapter.h>
#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/UserInputDevice.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/wupdlock.h>

#include "InstallDCSHooks.h"

#include <functional>
#include <limits>

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TabState.h>
#include "okAboutBox.h"
#include "okGamesListSettings.h"
#include "okTabsListSettings.h"
#include "okUserInputSettings.h"

using namespace OpenKneeboard;

okMainWindow::okMainWindow() : wxFrame(nullptr, wxID_ANY, "OpenKneeboard") {
  mDXR = DXResources::Create();
  mKneeboard = std::make_shared<KneeboardState>(this->GetHWND(), mDXR);
  AddEventListener(
    mKneeboard->evCurrentTabChangedEvent, &okMainWindow::OnTabChanged, this);

  InitUI();
}

okMainWindow::~okMainWindow() {
}

void okMainWindow::OnNotebookTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  mKneeboard->SetTabIndex(tab);
}

void okMainWindow::OnTabChanged(uint8_t tabIndex) {
  if (mNotebook->GetSelection() == tabIndex) {
    return;
  }
  mNotebook->SetSelection(tabIndex);
}

void okMainWindow::OnExit(wxCommandEvent& ev) {
  Close(true);
}

void okMainWindow::OnShowSettings(wxCommandEvent& ev) {
  const std::vector<std::function<wxWindow*(wxWindow*)>> factories {
    [&](wxWindow* p) {
      return new okTabsListSettings(p, mDXR, mKneeboard);
    },
    [&](wxWindow* p) {
      auto gamesList = mKneeboard->GetGamesList();
      return new okGamesListSettings(p, gamesList);
    },
    [&](wxWindow* p) {
      return new okUserInputSettings(p, mKneeboard->GetInputDevices());
    },
  };

  auto w = new wxFrame(this, wxID_ANY, _("Settings"));
  auto s = new wxBoxSizer(wxVERTICAL);

  auto nb = new wxNotebook(w, wxID_ANY);
  s->Add(nb, 1, wxEXPAND);

  for (const auto& factory: factories) {
    auto p = new wxPanel(nb, wxID_ANY);
    auto ui = factory(p);
    if (!ui) {
      continue;
    }

    auto ps = new wxBoxSizer(wxVERTICAL);
    ps->Add(ui, 1, wxEXPAND, 5);
    p->SetSizerAndFit(ps);

    nb->AddPage(p, ui->GetLabel());
  }

  w->SetSizerAndFit(s);
  w->Show(true);
}

void okMainWindow::UpdateTabs() {
  wxWindowUpdateLocker noUpdates(mNotebook);

  mNotebook->DeleteAllPages();

  auto selected = mKneeboard->GetCurrentTab();

  for (auto tabState: mKneeboard->GetTabs()) {
    auto ui = new okTab(mNotebook, mDXR, mKneeboard, tabState);
    mNotebook->AddPage(
      ui, tabState->GetRootTab()->GetTitle(), selected == tabState);
  }
}

void okMainWindow::InitUI() {
  SetIcon(wxIcon("appIcon"));

  auto menuBar = new wxMenuBar();
  {
    auto fileMenu = new wxMenu();
    menuBar->Append(fileMenu, _("&File"));

    fileMenu->Append(wxID_EXIT, _("E&xit"));
    Bind(wxEVT_MENU, &okMainWindow::OnExit, this, wxID_EXIT);
  }
  {
    auto editMenu = new wxMenu();
    menuBar->Append(editMenu, _("&Edit"));

    auto settingsId = wxNewId();
    editMenu->Append(settingsId, _("&Settings..."));
    Bind(wxEVT_MENU, &okMainWindow::OnShowSettings, this, settingsId);
  }
  {
    auto helpMenu = new wxMenu();
    menuBar->Append(helpMenu, _("&Help"));

    helpMenu->Append(wxID_ABOUT, _("&About"));
    Bind(
      wxEVT_MENU, [this](auto&) { okAboutBox(this); }, wxID_ABOUT);
  }
  SetMenuBar(menuBar);

  mNotebook = new wxNotebook(this, wxID_ANY);
  mNotebook->Bind(
    wxEVT_BOOKCTRL_PAGE_CHANGED, &okMainWindow::OnNotebookTabChanged, this);
  this->UpdateTabs();

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(mNotebook, 1, wxEXPAND);
  this->SetSizerAndFit(sizer);

  InstallDCSHooks();
}
