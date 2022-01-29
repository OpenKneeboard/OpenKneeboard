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

#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/wupdlock.h>

#include "Settings.h"
#include "okAboutBox.h"
#include "okDirectInputController.h"
#include "okEvents.h"
#include "okGameEventMailslotThread.h"
#include "okGamesList.h"
#include "okOpenVRThread.h"
#include "okSHMRenderer.h"
#include "okTab.h"
#include "okTabsList.h"

using namespace OpenKneeboard;

class okMainWindow::Impl {
 public:
  std::vector<okConfigurableComponent*> configurables;
  std::vector<okTab*> tabUIs;
  wxNotebook* notebook = nullptr;
  okTabsList* tabsList = nullptr;
  int currentTab = -1;
  Settings settings = Settings::Load();

  std::unique_ptr<okSHMRenderer> shmRenderer;
};

okMainWindow::okMainWindow()
  : wxFrame(nullptr, wxID_ANY, "OpenKneeboard"), p(std::make_unique<Impl>()) {
  (new okOpenVRThread())->Run();
  (new okGameEventMailslotThread(this))->Run();
  p->shmRenderer = std::make_unique<okSHMRenderer>();

  this->Bind(okEVT_GAME_EVENT, &okMainWindow::OnGameEvent, this);
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

  p->notebook = new wxNotebook(this, wxID_ANY);
  p->notebook->Bind(
    wxEVT_BOOKCTRL_PAGE_CHANGED, &okMainWindow::OnTabChanged, this);

  {
    auto tabs = new okTabsList(p->settings.Tabs);
    p->tabsList = tabs;
    p->configurables.push_back(tabs);
    UpdateTabs();
    tabs->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->settings.Tabs = tabs->GetSettings();
      p->settings.Save();
      this->UpdateTabs();
    });
  }

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(p->notebook, 1, wxEXPAND);
  this->SetSizerAndFit(sizer);

  UpdateSHM();

  {
    auto gl = new okGamesList(p->settings.Games);
    p->configurables.push_back(gl);

    gl->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->settings.Games = gl->GetSettings();
      p->settings.Save();
    });
  }

  {
    auto dipc = new okDirectInputController(p->settings.DirectInput);
    p->configurables.push_back(dipc);

    dipc->Bind(okEVT_PREVIOUS_TAB, &okMainWindow::OnPreviousTab, this);
    dipc->Bind(okEVT_NEXT_TAB, &okMainWindow::OnNextTab, this);
    dipc->Bind(okEVT_PREVIOUS_PAGE, &okMainWindow::OnPreviousPage, this);
    dipc->Bind(okEVT_NEXT_PAGE, &okMainWindow::OnNextPage, this);
    dipc->Bind(
      okEVT_TOGGLE_VISIBILITY, &okMainWindow::OnToggleVisibility, this);

    dipc->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->settings.DirectInput = dipc->GetSettings();
      p->settings.Save();
    });
  }
}

okMainWindow::~okMainWindow() {
}

void okMainWindow::OnTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  p->currentTab = tab;
  UpdateSHM();
}

void okMainWindow::OnGameEvent(wxThreadEvent& ev) {
  const auto ge = ev.GetPayload<GameEvent>();
  dprintf("GameEvent: '{}' = '{}'", ge.name, ge.value);
  for (auto tab: p->tabUIs) {
    tab->GetTab()->OnGameEvent(ge);
  }
}

void okMainWindow::UpdateSHM() {
  if (!p->shmRenderer) {
    return;
  }

  std::shared_ptr<Tab> tab;
  unsigned int pageIndex = 0;
  if (p->currentTab >= 0 && p->currentTab < p->tabUIs.size()) {
    auto tabUI = p->tabUIs.at(p->currentTab);
    tab = tabUI->GetTab();
    pageIndex = tabUI->GetPageIndex();
  }
  p->shmRenderer->Render(tab, pageIndex);
}

void okMainWindow::OnExit(wxCommandEvent& ev) {
  Close(true);
}

void okMainWindow::OnShowSettings(wxCommandEvent& ev) {
  auto w = new wxFrame(this, wxID_ANY, _("Settings"));
  auto s = new wxBoxSizer(wxVERTICAL);

  auto nb = new wxNotebook(w, wxID_ANY);
  s->Add(nb, 1, wxEXPAND);

  for (auto& component: p->configurables) {
    auto p = new wxPanel(nb, wxID_ANY);
    auto ui = component->GetSettingsUI(p);
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

void okMainWindow::OnPreviousTab(wxCommandEvent&) {
  p->notebook->AdvanceSelection(false);
}

void okMainWindow::OnNextTab(wxCommandEvent&) {
  p->notebook->AdvanceSelection(true);
}

void okMainWindow::OnPreviousPage(wxCommandEvent&) {
  p->tabUIs[p->currentTab]->PreviousPage();
}

void okMainWindow::OnNextPage(wxCommandEvent&) {
  p->tabUIs[p->currentTab]->NextPage();
}

void okMainWindow::OnToggleVisibility(wxCommandEvent&) {
  if (p->shmRenderer) {
    p->shmRenderer.reset();
    return;
  }

  p->shmRenderer = std::make_unique<okSHMRenderer>();
  UpdateSHM();
}

void okMainWindow::UpdateTabs() {
  auto tabs = p->tabsList->GetTabs();
  wxWindowUpdateLocker noUpdates(p->notebook);

  auto selected
    = p->currentTab >= 0 ? p->tabUIs[p->currentTab]->GetTab() : nullptr;
  p->currentTab = tabs.empty() ? -1 : 0;
  p->tabUIs.clear();
  p->notebook->DeleteAllPages();

  for (auto tab: tabs) {
    if (selected == tab) {
      p->currentTab = p->notebook->GetPageCount();
    }

    auto ui = new okTab(p->notebook, tab);
    p->tabUIs.push_back(ui);

    p->notebook->AddPage(ui, tab->GetTitle(), selected == tab);
    ui->Bind(okEVT_TAB_PIXELS_CHANGED, [this](auto) { this->UpdateSHM(); });
  }

  UpdateSHM();
}
