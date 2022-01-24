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
#include "okGamesList_SettingsUI.h"

#include <OpenKneeboard/dprint.h>
#include <wx/filedlg.h>
#include <wx/listbook.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>

#include "GetIconFromExecutable.h"
#include "okEvents.h"
#include "okGameInstanceSettings.h"
#include "okSelectExecutableDialog.h"

using namespace OpenKneeboard;

void okGamesList::SettingsUI::OnPathSelect(wxCommandEvent& ev) {
  auto path = std::filesystem::path(ev.GetString().ToStdWstring());
  int imageIndex = -1;
  auto ico = GetIconFromExecutable(path);
  auto imageList = mList->GetImageList();
  if (ico.IsOk()) {
    imageIndex = imageList->Add(ico);
  }
  for (const auto& game: mGamesList->mGames) {
    if (!game->MatchesPath(path)) {
      continue;
    }
    GameInstance instance {
      .name = game->GetUserFriendlyName(path).ToStdString(),
      .path = path,
      .game = game};
    mGamesList->mInstances.push_back(instance);
    mList->AddPage(
      new okGameInstanceSettings(mList, instance),
      instance.name,
      true,
      imageIndex);
  }
  auto dialog = dynamic_cast<wxDialog*>(ev.GetEventObject());
  if (!dialog) {
    dprintf("No dialog in {}", __FUNCTION__);
    return;
  }
  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
  dialog->Close();
}

void okGamesList::SettingsUI::OnAddGameButton(wxCommandEvent& ev) {
  okSelectExecutableDialog dialog(nullptr, wxID_ANY, _("Add Game"));
  dialog.Bind(okEVT_PATH_SELECTED, &SettingsUI::OnPathSelect, this);
  dialog.ShowModal();
}

void okGamesList::SettingsUI::OnRemoveGameButton(wxCommandEvent& ev) {
  auto page = mList->GetCurrentPage();
  if (!page) {
    return;
  }
  auto gameSettings = dynamic_cast<okGameInstanceSettings*>(page);
  if (!gameSettings) {
    dprint("Game list page is not an okGameInstanceSettings");
    return;
  }
  auto game = gameSettings->GetGameInstance();

  wxMessageDialog dialog(
    nullptr,
    fmt::format(
      _("Are you sure you want to remove '{}'?").ToStdString(), game.name),
    _("Remove game?"),
    wxYES_NO | wxNO_DEFAULT);
  auto result = dialog.ShowModal();
  if (result != wxID_YES) {
    return;
  }

  mList->DeletePage(mList->GetSelection());

  auto& instances = mGamesList->mInstances;
  for (auto it = instances.begin(); it != instances.end(); ++it) {
    if (it->path != game.path) {
      continue;
    }
    instances.erase(it);
    break;
  }
  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
}

okGamesList::SettingsUI::SettingsUI(wxWindow* parent, okGamesList* gamesList)
  : wxPanel(parent, wxID_ANY), mGamesList(gamesList) {
  this->SetLabel(_("Games"));
  mList = new wxListbook(this, wxID_ANY);
  mList->SetWindowStyleFlag(wxLB_LEFT);
  auto imageList = new wxImageList(32, 32);
  mList->AssignImageList(imageList);

  for (auto game: mGamesList->mInstances) {
    int imageIndex = -1;
    auto ico = GetIconFromExecutable(game.path);
    if (ico.IsOk()) {
      imageIndex = imageList->Add(ico);
    }
    mList->AddPage(
      new okGameInstanceSettings(mList, game), game.name, false, imageIndex);
  }

  auto add = new wxButton(this, wxID_ANY, _("&Add"));
  add->Bind(wxEVT_BUTTON, &SettingsUI::OnAddGameButton, this);
  auto remove = new wxButton(this, wxID_ANY, _("&Remove"));
  remove->Bind(wxEVT_BUTTON, &SettingsUI::OnRemoveGameButton, this);

  auto s = new wxBoxSizer(wxHORIZONTAL);
  s->Add(mList, 1, wxEXPAND | wxFIXED_MINSIZE, 5);
  s->Add(
    new wxStaticLine(
      this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL),
    0,
    wxEXPAND,
    5);

  auto buttons = new wxBoxSizer(wxVERTICAL);
  buttons->Add(add, 0, 0, 5);
  buttons->Add(remove, 0, 0, 5);
  buttons->AddStretchSpacer();
  s->Add(buttons, 0, wxEXPAND, 5);

  this->SetSizerAndFit(s);
}
