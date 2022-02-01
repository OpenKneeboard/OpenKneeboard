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
#include <OpenKneeboard/WintabTablet.h>
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
  std::vector<okConfigurableComponent*> mConfigurables;
  std::vector<okTab*> mTabUIs;
  wxNotebook* mNotebook = nullptr;
  okTabsList* mTabsList = nullptr;
  int mCurrentTab = -1;
  Settings mSettings = Settings::Load();

  std::unique_ptr<okSHMRenderer> mSHMRenderer;
  std::unique_ptr<WintabTablet> mTablet;
};

okMainWindow::okMainWindow()
  : wxFrame(nullptr, wxID_ANY, "OpenKneeboard"), p(std::make_unique<Impl>()) {
  (new okOpenVRThread())->Run();
  (new okGameEventMailslotThread(this))->Run();
  p->mSHMRenderer = std::make_unique<okSHMRenderer>();
  p->mTablet = std::make_unique<WintabTablet>(this->GetHWND());

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

  p->mNotebook = new wxNotebook(this, wxID_ANY);
  p->mNotebook->Bind(
    wxEVT_BOOKCTRL_PAGE_CHANGED, &okMainWindow::OnTabChanged, this);

  {
    auto tabs = new okTabsList(p->mSettings.Tabs);
    p->mTabsList = tabs;
    p->mConfigurables.push_back(tabs);
    UpdateTabs();
    tabs->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->mSettings.Tabs = tabs->GetSettings();
      p->mSettings.Save();
      this->UpdateTabs();
    });
  }

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(p->mNotebook, 1, wxEXPAND);
  this->SetSizerAndFit(sizer);

  UpdateSHM();

  {
    auto gl = new okGamesList(p->mSettings.Games);
    p->mConfigurables.push_back(gl);

    gl->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->mSettings.Games = gl->GetSettings();
      p->mSettings.Save();
    });
  }

  {
    auto dipc = new okDirectInputController(p->mSettings.DirectInput);
    p->mConfigurables.push_back(dipc);

    dipc->Bind(okEVT_PREVIOUS_TAB, &okMainWindow::OnPreviousTab, this);
    dipc->Bind(okEVT_NEXT_TAB, &okMainWindow::OnNextTab, this);
    dipc->Bind(okEVT_PREVIOUS_PAGE, &okMainWindow::OnPreviousPage, this);
    dipc->Bind(okEVT_NEXT_PAGE, &okMainWindow::OnNextPage, this);
    dipc->Bind(
      okEVT_TOGGLE_VISIBILITY, &okMainWindow::OnToggleVisibility, this);

    dipc->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->p->mSettings.DirectInput = dipc->GetSettings();
      p->mSettings.Save();
    });
  }
}

okMainWindow::~okMainWindow() {
}

WXLRESULT
okMainWindow::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
  if (*p->mTablet && p->mTablet->ProcessMessage(message, wParam, lParam)) {
    const auto state = p->mTablet->GetState();
    if (state.active) {
      // TODO: make tablet rotation configurable.
      // For now, assume tablet is rotated 90 degrees clockwise
      auto tabletSize = p->mTablet->GetSize();
      float x = tabletSize.height - state.y;
      float y = state.x;
      std::swap(tabletSize.width, tabletSize.height);

      const auto canvasSize = p->mSHMRenderer->GetCanvasSize();

      // scale to the render canvas
      const auto xScale
        = static_cast<float>(canvasSize.width) / tabletSize.width;
      const auto yScale
        = static_cast<float>(canvasSize.height) / tabletSize.height;
      const auto scale = std::max(xScale, yScale);
      x *= scale;
      y *= scale;
      p->mSHMRenderer->SetCursorPosition(x, y);
    } else {
      p->mSHMRenderer->HideCursor();
    }
    UpdateSHM();
  }
  return wxFrame::MSWWindowProc(message, wParam, lParam);
}

void okMainWindow::OnTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  p->mCurrentTab = tab;
  UpdateSHM();
}

void okMainWindow::OnGameEvent(wxThreadEvent& ev) {
  const auto ge = ev.GetPayload<GameEvent>();
  dprintf("GameEvent: '{}' = '{}'", ge.name, ge.value);
  for (auto tab: p->mTabUIs) {
    tab->GetTab()->OnGameEvent(ge);
  }
}

void okMainWindow::UpdateSHM() {
  if (!p->mSHMRenderer) {
    return;
  }

  std::shared_ptr<Tab> tab;
  unsigned int pageIndex = 0;
  if (p->mCurrentTab >= 0 && p->mCurrentTab < p->mTabUIs.size()) {
    auto tabUI = p->mTabUIs.at(p->mCurrentTab);
    tab = tabUI->GetTab();
    pageIndex = tabUI->GetPageIndex();
  }
  p->mSHMRenderer->Render(tab, pageIndex);
}

void okMainWindow::OnExit(wxCommandEvent& ev) {
  Close(true);
}

void okMainWindow::OnShowSettings(wxCommandEvent& ev) {
  auto w = new wxFrame(this, wxID_ANY, _("Settings"));
  auto s = new wxBoxSizer(wxVERTICAL);

  auto nb = new wxNotebook(w, wxID_ANY);
  s->Add(nb, 1, wxEXPAND);

  for (auto& component: p->mConfigurables) {
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
  p->mNotebook->AdvanceSelection(false);
}

void okMainWindow::OnNextTab(wxCommandEvent&) {
  p->mNotebook->AdvanceSelection(true);
}

void okMainWindow::OnPreviousPage(wxCommandEvent&) {
  p->mTabUIs[p->mCurrentTab]->PreviousPage();
}

void okMainWindow::OnNextPage(wxCommandEvent&) {
  p->mTabUIs[p->mCurrentTab]->NextPage();
}

void okMainWindow::OnToggleVisibility(wxCommandEvent&) {
  if (p->mSHMRenderer) {
    p->mSHMRenderer.reset();
    return;
  }

  p->mSHMRenderer = std::make_unique<okSHMRenderer>();
  UpdateSHM();
}

void okMainWindow::UpdateTabs() {
  auto tabs = p->mTabsList->GetTabs();
  wxWindowUpdateLocker noUpdates(p->mNotebook);

  auto selected
    = p->mCurrentTab >= 0 ? p->mTabUIs[p->mCurrentTab]->GetTab() : nullptr;
  p->mCurrentTab = tabs.empty() ? -1 : 0;
  p->mTabUIs.clear();
  p->mNotebook->DeleteAllPages();

  for (auto tab: tabs) {
    if (selected == tab) {
      p->mCurrentTab = p->mNotebook->GetPageCount();
    }

    auto ui = new okTab(p->mNotebook, tab);
    p->mTabUIs.push_back(ui);

    p->mNotebook->AddPage(ui, tab->GetTitle(), selected == tab);
    ui->Bind(okEVT_TAB_PIXELS_CHANGED, [this](auto) { this->UpdateSHM(); });
  }

  UpdateSHM();
}
