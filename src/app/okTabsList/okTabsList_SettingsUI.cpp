#include "okTabsList_SettingsUI.h"

#include <wx/listctrl.h>

#include "okEvents.h"
#include "okTabsList_SharedState.h"

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
  auto remove = new wxButton(this, wxID_ANY, _("&Remove"));
  remove->Bind(wxEVT_BUTTON, &SettingsUI::OnRemoveItem, this);
  auto up = new wxButton(this, wxID_ANY, _("Move &Up"));
  auto down = new wxButton(this, wxID_ANY, _("Move &Down"));

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

void okTabsList::SettingsUI::OnRemoveItem(wxCommandEvent& ev) {
  ev.StopPropagation();

  auto index = mList->GetFirstSelected();
  if (index == -1) {
    return;
  }
  if (index >= s->Tabs.size()) {
    return;
  }

  s->Tabs.erase(s->Tabs.begin() + index);
  mList->DeleteItem(index);

  wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED));
}
