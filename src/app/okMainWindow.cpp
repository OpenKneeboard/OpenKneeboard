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

#include <OpenKneeboard/dprint.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/wupdlock.h>

#include <limits>

#include "okAboutBox.h"
#include "okDirectInputController.h"
#include "okEvents.h"
#include "okGameEventMailslotThread.h"
#include "okGamesList.h"

using namespace OpenKneeboard;

okMainWindow::okMainWindow() : wxFrame(nullptr, wxID_ANY, "OpenKneeboard") {
  mDXResources = DXResources::Create();

  (new okOpenVRThread())->Run();
  (new okGameEventMailslotThread(this))->Run();
  mSHMRenderer = std::make_unique<okSHMRenderer>(this->GetHWND(), mDXResources);
  mTablet = std::make_unique<WintabTablet>(this->GetHWND());

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

  mNotebook = new wxNotebook(this, wxID_ANY);
  mNotebook->Bind(
    wxEVT_BOOKCTRL_PAGE_CHANGED, &okMainWindow::OnTabChanged, this);

  {
    auto tabs = new okTabsList(mSettings.Tabs, mDXResources);
    mTabsList = tabs;
    mConfigurables.push_back(tabs);
    UpdateTabs();
    tabs->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->mSettings.Tabs = tabs->GetSettings();
      mSettings.Save();
      this->UpdateTabs();
    });
  }

  auto sizer = new wxBoxSizer(wxVERTICAL);
  sizer->Add(mNotebook, 1, wxEXPAND);
  this->SetSizerAndFit(sizer);

  UpdateSHM();

  {
    auto gl = new okGamesList(mSettings.Games);
    mConfigurables.push_back(gl);

    gl->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->mSettings.Games = gl->GetSettings();
      mSettings.Save();
    });
  }

  {
    auto dipc = new okDirectInputController(mSettings.DirectInput);
    mConfigurables.push_back(dipc);

    dipc->Bind(okEVT_PREVIOUS_TAB, &okMainWindow::OnPreviousTab, this);
    dipc->Bind(okEVT_NEXT_TAB, &okMainWindow::OnNextTab, this);
    dipc->Bind(okEVT_PREVIOUS_PAGE, &okMainWindow::OnPreviousPage, this);
    dipc->Bind(okEVT_NEXT_PAGE, &okMainWindow::OnNextPage, this);
    dipc->Bind(
      okEVT_TOGGLE_VISIBILITY, &okMainWindow::OnToggleVisibility, this);

    dipc->Bind(okEVT_SETTINGS_CHANGED, [=](auto&) {
      this->mSettings.DirectInput = dipc->GetSettings();
      mSettings.Save();
    });
  }
}

okMainWindow::~okMainWindow() {
}

WXLRESULT
okMainWindow::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
  if (*mTablet && mTablet->ProcessMessage(message, wParam, lParam)) {
    const auto state = mTablet->GetState();
    if (state.active) {
      // TODO: make tablet rotation configurable.
      // For now, assume tablet is rotated 90 degrees clockwise
      auto tabletLimits = mTablet->GetLimits();
      float x = tabletLimits.y - state.y;
      float y = state.x;
      std::swap(tabletLimits.x, tabletLimits.y);

      const auto canvasSize = mSHMRenderer->GetCanvasSize();

      // scale to the render canvas
      const auto xScale = static_cast<float>(canvasSize.width) / tabletLimits.x;
      const auto yScale
        = static_cast<float>(canvasSize.height) / tabletLimits.y;
      const auto scale = std::max(xScale, yScale);
      x *= scale;
      y *= scale;
      mSHMRenderer->SetCursorPosition(x, y);

      if (mCurrentTab >= 0) {
        const auto pressure
          = static_cast<float>(state.pressure) / tabletLimits.pressure;

        const auto clientRect = mSHMRenderer->GetClientRect();
        CursorEvent event {
          .TouchState = pressure >= std::numeric_limits<float>::epsilon()
            ? CursorTouchState::TOUCHING_SURFACE
            : CursorTouchState::NEAR_SURFACE,
          .x = x - clientRect.left,
          .y = y - clientRect.top,
          .pressure = pressure,
          .buttons = state.buttons,
        };

        if (
          x >= clientRect.left && x <= clientRect.right && y >= clientRect.top
          && y <= clientRect.bottom) {
          event.PositionState = CursorPositionState::IN_CLIENT_RECT;
        } else {
          event.PositionState = CursorPositionState::NOT_IN_CLIENT_RECT;
        }

        mTabUIs.at(mCurrentTab)->OnCursorEvent(event);
      }
    } else {
      mSHMRenderer->HideCursor();
      if (mCurrentTab >= 0) {
        mTabUIs.at(mCurrentTab)
          ->OnCursorEvent(
            {.PositionState = CursorPositionState::NOT_IN_CLIENT_RECT,
             .TouchState = CursorTouchState::NOT_NEAR_SURFACE});
      }
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
  mCurrentTab = tab;
  UpdateSHM();
}

void okMainWindow::OnGameEvent(wxThreadEvent& ev) {
  const auto ge = ev.GetPayload<GameEvent>();
  dprintf("GameEvent: '{}' = '{}'", ge.name, ge.value);
  for (auto tab: mTabUIs) {
    tab->GetTab()->OnGameEvent(ge);
  }
}

void okMainWindow::UpdateSHM() {
  if (!mSHMRenderer) {
    return;
  }

  std::shared_ptr<Tab> tab;
  unsigned int pageIndex = 0;
  if (mCurrentTab >= 0 && mCurrentTab < mTabUIs.size()) {
    auto tabUI = mTabUIs.at(mCurrentTab);
    tab = tabUI->GetTab();
    pageIndex = tabUI->GetPageIndex();
  }
  mSHMRenderer->Render(tab, pageIndex);
}

void okMainWindow::OnExit(wxCommandEvent& ev) {
  Close(true);
}

void okMainWindow::OnShowSettings(wxCommandEvent& ev) {
  auto w = new wxFrame(this, wxID_ANY, _("Settings"));
  auto s = new wxBoxSizer(wxVERTICAL);

  auto nb = new wxNotebook(w, wxID_ANY);
  s->Add(nb, 1, wxEXPAND);

  for (auto& component: mConfigurables) {
    auto p = new wxPanel(nb, wxID_ANY);
    auto ui = component->GetSettingsUI(p);
    if (!ui) {
      continue;
    }

    auto ps = new wxBoxSizer(wxVERTICAL);
    ps->Add(ui, 1, wxEXPAND, 5);
    SetSizerAndFit(ps);

    nb->AddPage(p, ui->GetLabel());
  }

  w->SetSizerAndFit(s);
  w->Show(true);
}

void okMainWindow::OnPreviousTab(wxCommandEvent&) {
  mNotebook->AdvanceSelection(false);
}

void okMainWindow::OnNextTab(wxCommandEvent&) {
  mNotebook->AdvanceSelection(true);
}

void okMainWindow::OnPreviousPage(wxCommandEvent&) {
  mTabUIs[mCurrentTab]->PreviousPage();
}

void okMainWindow::OnNextPage(wxCommandEvent&) {
  mTabUIs[mCurrentTab]->NextPage();
}

void okMainWindow::OnToggleVisibility(wxCommandEvent&) {
  if (mSHMRenderer) {
    mSHMRenderer.reset();
    return;
  }

  mSHMRenderer = std::make_unique<okSHMRenderer>(this->GetHWND(), mDXResources);
  UpdateSHM();
}

void okMainWindow::UpdateTabs() {
  auto tabs = mTabsList->GetTabs();
  wxWindowUpdateLocker noUpdates(mNotebook);

  auto selected = mCurrentTab >= 0 ? mTabUIs[mCurrentTab]->GetTab() : nullptr;
  mCurrentTab = tabs.empty() ? -1 : 0;
  mTabUIs.clear();
  mNotebook->DeleteAllPages();

  for (auto tab: tabs) {
    if (selected == tab) {
      mCurrentTab = mNotebook->GetPageCount();
    }

    auto ui = new okTab(mNotebook, tab, mDXResources);
    mTabUIs.push_back(ui);

    mNotebook->AddPage(ui, tab->GetTitle(), selected == tab);
    ui->Bind(okEVT_TAB_PIXELS_CHANGED, [this](auto) { this->UpdateSHM(); });
  }

  UpdateSHM();
}
