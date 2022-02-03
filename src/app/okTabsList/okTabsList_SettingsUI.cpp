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
#include "okTabsList_SettingsUI.h"

#include <OpenKneeboard/dprint.h>
#include <wx/choicdlg.h>
#include <wx/listctrl.h>
#include <wx/wupdlock.h>

#include "TabTypes.h"
#include "okEvents.h"
#include "okTabsList_SharedState.h"

using namespace OpenKneeboard;

okTabsList::SettingsUI::SettingsUI(
  wxWindow* parent,
  const std::shared_ptr<SharedState>& state)
  : wxPanel(parent, wxID_ANY), s(state) {
  this->SetLabel(_("Tabs"));

  mList = new wxListView(this, wxID_ANY);
  mList->SetWindowStyle(wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
  mList->AppendColumn(_("Title"));

  for (const auto& tab: state->tabs) {
    const auto row = mList->GetItemCount();

    mList->InsertItem(row, tab->GetTitle());
  }

  mList->SetColumnWidth(0, wxLIST_AUTOSIZE);

  auto add = new wxButton(this, wxID_ANY, _("&Add"));
  add->Bind(wxEVT_BUTTON, &SettingsUI::OnAddTab, this);
  auto remove = new wxButton(this, wxID_ANY, _("&Remove"));
  remove->Bind(wxEVT_BUTTON, &SettingsUI::OnRemoveTab, this);
  auto up = new wxButton(this, wxID_ANY, _("Move &Up"));
  up->Bind(wxEVT_BUTTON, &SettingsUI::OnMoveTabUp, this);
  auto down = new wxButton(this, wxID_ANY, _("Move &Down"));
  down->Bind(wxEVT_BUTTON, &SettingsUI::OnMoveTabDown, this);

  auto buttons = new wxBoxSizer(wxVERTICAL);
  buttons->Add(add, 0, 0, 5);
  buttons->Add(remove, 0, 0, 5);
  buttons->Add(up, 0, 0, 5);
  buttons->Add(down, 0, 0, 5);
  buttons->AddStretchSpacer();

  auto s = new wxBoxSizer(wxHORIZONTAL);
  s->Add(mList, 0, wxEXPAND, 5);
  s->AddSpacer(5);
  s->Add(buttons, 0, wxEXPAND, 5);
  s->AddStretchSpacer();
  this->SetSizerAndFit(s);
}

okTabsList::SettingsUI::~SettingsUI() {
}

void okTabsList::SettingsUI::OnAddTab(wxCommandEvent& ev) {
  ev.StopPropagation();

  std::vector<wxString> choices = {
#define IT(label, _) label,
    OPENKNEEBOARD_TAB_TYPES
#undef IT
  };

  wxSingleChoiceDialog tabTypeDialog(
    this,
    _("What kind of tab do you want to add?"),
    _("Add Tab"),
    choices.size(),
    choices.data(),
    nullptr,
    wxCHOICEDLG_STYLE | wxOK | wxCANCEL);

  if (tabTypeDialog.ShowModal() == wxID_CANCEL) {
    return;
  }

  switch (tabTypeDialog.GetSelection()) {
#define IT(_, type) \
  case TABTYPE_IDX_##type: \
    InsertTab(create_tab<type##Tab>(nullptr, s->dxr)); \
    return;
    OPENKNEEBOARD_TAB_TYPES
#undef IT
    default:
      dprintf("Invalid tab type index: {}", tabTypeDialog.GetSelection());
      return;
  }
}

void okTabsList::SettingsUI::OnRemoveTab(wxCommandEvent& ev) {
  ev.StopPropagation();

  auto index = mList->GetFirstSelected();
  if (index == -1) {
    return;
  }

  s->tabs.erase(s->tabs.begin() + index);
  mList->DeleteItem(index);

  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
}

void okTabsList::SettingsUI::OnMoveTabUp(wxCommandEvent& ev) {
  ev.StopPropagation();

  MoveTab(Direction::UP);
}

void okTabsList::SettingsUI::OnMoveTabDown(wxCommandEvent& ev) {
  ev.StopPropagation();

  MoveTab(Direction::DOWN);
}

void okTabsList::SettingsUI::MoveTab(Direction direction) {
  auto index = mList->GetFirstSelected();

  if (index == -1) {
    return;
  }

  auto swapIndex = direction == Direction::UP ? index - 1 : index + 1;
  if (swapIndex < 0 || swapIndex >= mList->GetItemCount()) {
    return;
  }

  wxWindowUpdateLocker freezer(mList);

  auto& tabs = s->tabs;
  auto tab = tabs.at(index);
  std::swap(tabs[index], tabs[swapIndex]);
  mList->DeleteItem(index);
  index = mList->InsertItem(swapIndex, tab->GetTitle());
  mList->Select(index);

  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
}

void okTabsList::SettingsUI::InsertTab(const std::shared_ptr<Tab>& tab) {
  if (!tab) {
    return;
  }
  auto insertAt = mList->GetFirstSelected();
  if (insertAt == -1) {
    insertAt = 0;
  }

  s->tabs.insert(s->tabs.begin() + insertAt, tab);

  wxWindowUpdateLocker freezer(mList);
  mList->InsertItem(insertAt, tab->GetTitle());
  mList->Select(insertAt);

  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
}
