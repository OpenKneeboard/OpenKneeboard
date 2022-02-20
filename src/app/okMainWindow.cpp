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

#include <functional>
#include <limits>

#include "GameEventServer.h"
#include "InterprocessRenderer.h"
#include "KneeboardState.h"
#include "OpenVROverlay.h"
#include "TabState.h"
#include "okAboutBox.h"
#include "okGamesListSettings.h"
#include "okTabsListSettings.h"
#include "okUserInputSettings.h"

using namespace OpenKneeboard;

okMainWindow::okMainWindow() : wxFrame(nullptr, wxID_ANY, "OpenKneeboard") {
  mDXR = DXResources::Create();
  mKneeboard = std::make_shared<KneeboardState>();
  mInterprocessRenderer
    = std::make_unique<InterprocessRenderer>(mDXR, mKneeboard, this->GetHWND());

  mGamesList = std::make_unique<GamesList>(mSettings.Games);
  mDirectInput = std::make_unique<DirectInputAdapter>(mSettings.DirectInputV2);
  mTabletInput = std::make_unique<TabletInputAdapter>(
    this->GetHWND(), mKneeboard, mSettings.TabletInput);
  mTabsList = std::make_unique<TabsList>(mDXR, mKneeboard, mSettings.Tabs);

  AddEventListener(
    mDirectInput->evUserActionEvent, &okMainWindow::OnUserAction, this);
  AddEventListener(
    mTabletInput->evUserActionEvent, &okMainWindow::OnUserAction, this);

  mOpenVRThread = std::jthread(
    [](std::stop_token stopToken) { OpenVROverlay().Run(stopToken); });
  mGameEventThread = std::jthread([this](std::stop_token stopToken) {
    GameEventServer server;
    this->AddEventListener(
      server.evGameEvent, &okMainWindow::PostGameEvent, this);
    server.Run(stopToken);
  });

  InitUI();
}

okMainWindow::~okMainWindow() {
}

void okMainWindow::OnUserAction(UserAction action) {
  switch (action) {
    case UserAction::PREVIOUS_TAB:
      mNotebook->AdvanceSelection(false);
      return;
    case UserAction::NEXT_TAB:
      mNotebook->AdvanceSelection(true);
      return;
    case UserAction::PREVIOUS_PAGE:
      mKneeboard->PreviousPage();
      return;
    case UserAction::NEXT_PAGE:
      mKneeboard->NextPage();
      return;
    case UserAction::TOGGLE_VISIBILITY:
      this->OnToggleVisibility();
      return;
  }
  OPENKNEEBOARD_BREAK;
}

void okMainWindow::OnTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  mKneeboard->SetTabIndex(tab);
}

void okMainWindow::PostGameEvent(const GameEvent& ge) {
  if (ge.name == GameEvent::EVT_REMOTE_USER_ACTION) {
#define IT(ACTION) \
  if (ge.value == #ACTION) { \
    OnUserAction(UserAction::ACTION); \
    return; \
  }
    OPENKNEEBOARD_USER_ACTIONS
#undef IT
    return;
  }

  mKneeboard->PostGameEvent(ge);
  mKneeboard->evFlushEvent.Emit();
}

void okMainWindow::OnExit(wxCommandEvent& ev) {
  Close(true);
}

void okMainWindow::OnShowSettings(wxCommandEvent& ev) {
  const std::vector<std::function<wxWindow*(wxWindow*)>> factories {
    [&](wxWindow* p) {
      auto it = new okTabsListSettings(p, mDXR, mKneeboard);
      AddEventListener(it->evSettingsChangedEvent, [&] {
        mSettings.Tabs = mTabsList->GetSettings();
        mSettings.Save();
        this->UpdateTabs();
      });
      return it;
    },
    [&](wxWindow* p) {
      auto it = new okGamesListSettings(p, mGamesList.get());
      AddEventListener(it->evSettingsChangedEvent, [&] {
        mSettings.Games = mGamesList->GetSettings();
        mSettings.Save();
      });
      return it;
    },
    [&](wxWindow* p) {
      std::vector<std::shared_ptr<UserInputDevice>> devices;
      for (const auto& subDevices:
           {mTabletInput->GetDevices(), mDirectInput->GetDevices()}) {
        devices.reserve(devices.size() + subDevices.size());
        for (const auto& device: subDevices) {
          devices.push_back(device);
        }
      }
      auto it = new okUserInputSettings(p, devices);
      AddEventListener(it->evSettingsChangedEvent, [&] {
        mSettings.DirectInputV2 = mDirectInput->GetSettings();
        mSettings.TabletInput = mTabletInput->GetSettings();
        mSettings.Save();
      });
      return it;
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

void okMainWindow::OnToggleVisibility() {
  if (mInterprocessRenderer) {
    mInterprocessRenderer.reset();
    return;
  }

  mInterprocessRenderer
    = std::make_unique<InterprocessRenderer>(mDXR, mKneeboard, this->GetHWND());
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
    wxEVT_BOOKCTRL_PAGE_CHANGED, &okMainWindow::OnTabChanged, this);
  this->UpdateTabs();

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(mNotebook, 1, wxEXPAND);
  this->SetSizerAndFit(sizer);
}
