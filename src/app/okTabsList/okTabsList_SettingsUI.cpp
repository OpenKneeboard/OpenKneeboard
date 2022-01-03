#include "okTabsList_SettingsUI.h"

#include <ShlObj.h>
#include <wx/choicdlg.h>
#include <wx/dirdlg.h>
#include <wx/listctrl.h>
#include <wx/wupdlock.h>

#include "OpenKneeboard/dprint.h"
#include "okEvents.h"
#include "okTabsList_SharedState.h"
#include "TabTypes.h"

using namespace OpenKneeboard;

okTabsList::SettingsUI::SettingsUI(
  wxWindow* parent,
  const std::shared_ptr<SharedState>& state)
  : wxPanel(parent, wxID_ANY), s(state) {
  this->SetLabel(_("Tabs"));

  mList = new wxListView(this, wxID_ANY);
  mList->SetWindowStyle(wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
  mList->AppendColumn(_("Title"));

  for (const auto& tab: state->Tabs) {
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
    TAB_TYPES
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
    case TABTYPE_IDX_Folder:
      InsertFolderTab();
      return;
#define IT(_, type) \
  case TABTYPE_IDX_##type: \
    InsertTab(std::make_shared<type##Tab>()); \
    return;
      ZERO_CONFIG_TAB_TYPES
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

  s->Tabs.erase(s->Tabs.begin() + index);
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

  auto& tabs = s->Tabs;
  auto tab = tabs.at(index);
  std::swap(tabs[index], tabs[swapIndex]);
  mList->DeleteItem(index);
  index = mList->InsertItem(swapIndex, tab->GetTitle());
  mList->Select(index);

  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
}

void okTabsList::SettingsUI::InsertTab(const std::shared_ptr<Tab>& tab) {
  auto insertAt = mList->GetFirstSelected();
  if (insertAt == -1) {
    insertAt = 0;
  }

  s->Tabs.insert(s->Tabs.begin() + insertAt, tab);

  wxWindowUpdateLocker freezer(mList);
  mList->InsertItem(insertAt, tab->GetTitle());
  mList->Select(insertAt);

  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
}

void okTabsList::SettingsUI::InsertFolderTab() {
  wxDirDialog dialog(
    nullptr, _("Add Folder Tab"), {}, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

  wchar_t* userDir = nullptr;
  if (SHGetKnownFolderPath(FOLDERID_Documents, NULL, NULL, &userDir) == S_OK) {
    dialog.SetPath(userDir);
  }

  if (dialog.ShowModal() == wxID_CANCEL) {
    return;
  }

  std::filesystem::path path(dialog.GetPath().ToStdWstring());
  if (!std::filesystem::is_directory(path)) {
    return;
  }

  InsertTab(std::make_shared<FolderTab>(path.stem().string(), path));
}
