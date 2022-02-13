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

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/wupdlock.h>

#include <functional>
#include <limits>

#include "GameEventServer.h"
#include "KneeboardState.h"
#include "OpenVROverlay.h"
#include "TabState.h"
#include "okAboutBox.h"
#include "okDirectInputController.h"
#include "okEvents.h"
#include "okGamesList.h"

using namespace OpenKneeboard;

okMainWindow::okMainWindow() : wxFrame(nullptr, wxID_ANY, "OpenKneeboard") {
  mDXResources = DXResources::Create();
  mKneeboard = std::make_shared<KneeboardState>();
  mSHMRenderer = std::make_unique<okSHMRenderer>(
    mDXResources, mKneeboard, this->GetHWND());
  mTablet = std::make_unique<WintabTablet>(this->GetHWND());

  InitUI();

  if (*mTablet) {
    mCursorEventTimer.Bind(
      wxEVT_TIMER, [this](auto&) { this->FlushCursorEvents(); });
    mCursorEventTimer.Start(1000 / TabletCursorRenderHz);
  }

  mOpenVRThread = std::jthread(
    [](std::stop_token stopToken) { OpenVROverlay().Run(stopToken); });
  mGameEventThread = std::jthread([this](std::stop_token stopToken) {
    GameEventServer server;
    this->AddEventListener(
      server.evGameEvent, &okMainWindow::PostGameEvent, this);
    server.Run(stopToken);
  });

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

    AddEventListener(
      dipc->evUserAction,
      [=](UserAction action) {
        switch(action) {
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
        return;
      }
    );

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
      // in most cases, we use `std::min` - that would be for fitting the tablet
      // in the canvas bounds, but we want to fit the canvas in the tablet, so
      // doing the opposite
      const auto scale = std::max(scaleX, scaleY);

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
        event.mPositionState = CursorPositionState::IN_CONTENT_RECT;
      } else {
        event.mPositionState = CursorPositionState::NOT_IN_CONTENT_RECT;
      }

      mBufferedCursorEvents.push_back(event);
    } else {
      mBufferedCursorEvents.push_back({});
    }
  }
  return wxFrame::MSWWindowProc(message, wParam, lParam);
}

void okMainWindow::FlushCursorEvents() {
  if (mBufferedCursorEvents.empty()) {
    return;
  }

  for (const auto& event: mBufferedCursorEvents) {
    mKneeboard->evCursorEvent(event);
  }
  mBufferedCursorEvents.clear();

  mKneeboard->evFlushEvent();
}

void okMainWindow::OnTabChanged(wxBookCtrlEvent& ev) {
  const auto tab = ev.GetSelection();
  if (tab == wxNOT_FOUND) {
    return;
  }
  mKneeboard->SetTabIndex(tab);
}

void okMainWindow::PostGameEvent(const GameEvent& ge) {
  mKneeboard->PostGameEvent(ge);
  mKneeboard->evFlushEvent();
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

void okMainWindow::OnToggleVisibility() {
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

void okMainWindow::InitUI() {
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
}
