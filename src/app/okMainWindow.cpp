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

#include <functional>
#include <limits>

#include "KneeboardState.h"
#include "TabState.h"
#include "okAboutBox.h"
#include "okDirectInputController.h"
#include "okEvents.h"
#include "okGameEventMailslotThread.h"
#include "okGamesList.h"

using namespace OpenKneeboard;

okMainWindow::okMainWindow() : wxFrame(nullptr, wxID_ANY, "OpenKneeboard") {
  mDXResources = DXResources::Create();
  mKneeboard = std::make_shared<KneeboardState>();

  (new okOpenVRThread())->Run();
  (new okGameEventMailslotThread(this))->Run();
  mSHMRenderer = std::make_unique<okSHMRenderer>(
    mDXResources, mKneeboard, this->GetHWND());
  mTablet = std::make_unique<WintabTablet>(this->GetHWND());

  this->Bind(okEVT_GAME_EVENT, &okMainWindow::PostGameEvent, this);
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
    auto tabs = new okTabsList(mDXResources, mKneeboard, mSettings.Tabs);
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
    // TODO: make okDirectInputController directly act on mKneeboardState
    dipc->Bind(okEVT_PREVIOUS_PAGE, [=](auto&) { mKneeboard->PreviousPage(); });
    dipc->Bind(okEVT_NEXT_PAGE, [=](auto&) { mKneeboard->NextPage(); });
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

      // Cursor events use content coordinates, but as as the content isn't at
      // the origin, we need a few transformations:

      // 1. scale to canvas size
      auto canvasSize = mKneeboard->GetCanvasSize();

      const auto scaleX = static_cast<float>(canvasSize.width) / tabletLimits.x;
      const auto scaleY
        = static_cast<float>(canvasSize.height) / tabletLimits.y;
      const auto scale = std::min(scaleX, scaleY);

      x *= scale;
      y *= scale;

      // 2. translate to content origgin

      const auto contentRenderRect = mKneeboard->GetContentRenderRect();
      x -= contentRenderRect.left;
      y -= contentRenderRect.top;

      // 3. scale to content size;
      const auto contentNativeSize = mKneeboard->GetContentNativeSize();
      const auto contentScale = contentNativeSize.width
        / (contentRenderRect.right - contentRenderRect.left);
      x *= contentScale;
      y *= contentScale;

      CursorEvent event {
        .mTouchState = (state.penButtons & 1)
          ? CursorTouchState::TOUCHING_SURFACE
          : CursorTouchState::NEAR_SURFACE,
        .mX = x,
        .mY = y,
        .mPressure = static_cast<float>(state.pressure) / tabletLimits.pressure,
        .mButtons = state.penButtons,
      };

      if (
        x >= 0 && x <= contentNativeSize.width && y >= 0
        && y <= contentNativeSize.height) {
        event.mPositionState = CursorPositionState::IN_CLIENT_RECT;
      } else {
        event.mPositionState = CursorPositionState::NOT_IN_CLIENT_RECT;
      }

      mKneeboard->evCursorEvent(event);
    } else {
      mKneeboard->evCursorEvent({});
    }
  }
  return wxFrame::MSWWindowProc(message, wParam, lParam);
}

void okMainWindow::OnTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  mKneeboard->SetTabIndex(tab);
}

void okMainWindow::PostGameEvent(wxThreadEvent& ev) {
  const auto ge = ev.GetPayload<GameEvent>();
  mKneeboard->PostGameEvent(ge);
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
    p->SetSizerAndFit(ps);

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

void okMainWindow::OnToggleVisibility(wxCommandEvent&) {
  if (mSHMRenderer) {
    mSHMRenderer.reset();
    return;
  }

  mSHMRenderer = std::make_unique<okSHMRenderer>(
    mDXResources, mKneeboard, this->GetHWND());
}

void okMainWindow::UpdateTabs() {
  wxWindowUpdateLocker noUpdates(mNotebook);

  mNotebook->DeleteAllPages();

  auto selected = mKneeboard->GetCurrentTab();

  for (auto tabState: mKneeboard->GetTabs()) {
    auto ui = new okTab(mNotebook, mDXResources, mKneeboard, tabState);
    mNotebook->AddPage(
      ui, tabState->GetRootTab()->GetTitle(), selected == tabState);
  }
}
