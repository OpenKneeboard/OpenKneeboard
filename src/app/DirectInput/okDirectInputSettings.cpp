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
#include "okDirectInputSettings.h"

#include <OpenKneeboard/DirectInputBinding.h>
#include <OpenKneeboard/DirectInputButtonEvent.h>
#include <OpenKneeboard/GetDirectInputDevices.h>
#include <OpenKneeboard/utf8.h>
#include <fmt/format.h>
#include <wx/gbsizer.h>
#include <wx/scopeguard.h>

#include "scope_guard.h"

using namespace OpenKneeboard;

wxDEFINE_EVENT(okEVT_DI_CLEAR_BINDING_BUTTON, wxCommandEvent);

struct okDirectInputSettings::DeviceButtons {
  wxButton* previousTab = nullptr;
  wxButton* nextTab = nullptr;
  wxButton* previousPage = nullptr;
  wxButton* nextPage = nullptr;
  wxButton* toggleVisibility = nullptr;

  wxButton* Get(OpenKneeboard::UserAction action) {
    switch (action) {
      case UserAction::PREVIOUS_TAB:
        return previousTab;
      case UserAction::NEXT_TAB:
        return nextTab;
      case UserAction::PREVIOUS_PAGE:
        return previousPage;
      case UserAction::NEXT_PAGE:
        return nextPage;
      case UserAction::TOGGLE_VISIBILITY:
        return toggleVisibility;
    }
    OPENKNEEBOARD_BREAK;
    return nullptr;
  };
};

wxButton* okDirectInputSettings::CreateBindButton(
  wxWindow* parent,
  int deviceIndex,
  UserAction action) {
  const auto& device = mDevices.at(deviceIndex);
  auto label = _("Bind");
  for (auto binding: mDIController->GetBindings()) {
    if (
      binding.instanceGuid == device.guidInstance && binding.action == action) {
      label = fmt::format(
        fmt::runtime(_("Button {}").ToStdString()), binding.buttonIndex + 1);
      break;
    }
  }
  auto button = new wxButton(parent, wxID_ANY, label);
  button->Bind(wxEVT_BUTTON, [=](auto& ev) {
    this->OnBindButton(ev, deviceIndex, action);
  });
  return button;
}

okDirectInputSettings::okDirectInputSettings(
  wxWindow* parent,
  DirectInputAdapter* controller)
  : wxPanel(parent, wxID_ANY), mDIController(controller) {
  this->SetLabel(_("DirectInput"));
  mDevices = GetDirectInputDevices(controller->GetDirectInput());

  auto sizer = new wxBoxSizer(wxVERTICAL);

  auto panel = new wxPanel(this, wxID_ANY);
  sizer->Add(panel, 0, wxEXPAND);

  auto grid = new wxGridBagSizer(5, 5);
  grid->AddGrowableCol(0);

  auto bold = GetFont().MakeBold();
  for (const auto& column:
       {_("Device"),
        _("Show/Hide"),
        _("Previous Tab"),
        _("Next Tab"),
        _("Previous Page"),
        _("Next Page")}) {
    auto label = new wxStaticText(panel, wxID_ANY, column);
    label->SetFont(bold);
    grid->Add(label);
  }

  for (int i = 0; i < mDevices.size(); ++i) {
    const auto& device = mDevices.at(i);
    const auto row = i + 1;// headers

    auto label = new wxStaticText(panel, wxID_ANY, device.tszInstanceName);
    grid->Add(label, wxGBPosition(row, 0));

    auto toggleVisibility
      = CreateBindButton(panel, i, UserAction::TOGGLE_VISIBILITY);
    grid->Add(toggleVisibility, wxGBPosition(row, 1));

    auto previousTab = CreateBindButton(panel, i, UserAction::PREVIOUS_TAB);
    grid->Add(previousTab, wxGBPosition(row, 2));

    auto nextTab = CreateBindButton(panel, i, UserAction::NEXT_TAB);
    grid->Add(nextTab, wxGBPosition(row, 3));

    auto previousPage = CreateBindButton(panel, i, UserAction::PREVIOUS_PAGE);
    grid->Add(previousPage, wxGBPosition(row, 4));

    auto nextPage = CreateBindButton(panel, i, UserAction::NEXT_PAGE);
    grid->Add(nextPage, wxGBPosition(row, 5));

    mDeviceButtons.push_back({
      .previousTab = previousTab,
      .nextTab = nextTab,
      .previousPage = previousPage,
      .nextPage = nextPage,
      .toggleVisibility = toggleVisibility,
    });
  }
  grid->SetCols(5);
  panel->SetSizerAndFit(grid);

  sizer->AddStretchSpacer();
  this->SetSizerAndFit(sizer);
  Refresh();
}

okDirectInputSettings::~okDirectInputSettings() {
}

wxDialog* okDirectInputSettings::CreateBindInputDialog(
  bool haveExistingBinding) {
  auto d = new wxDialog(this, wxID_ANY, _("Bind Inputs"));

  auto s = new wxBoxSizer(wxVERTICAL);

  s->Add(
    new wxStaticText(d, wxID_ANY, _("Press button to bind input...")),
    0,
    wxALL,
    5);
  auto bs = d->CreateButtonSizer(wxCANCEL | wxNO_DEFAULT);
  s->Add(bs, 0, wxALL, 5);

  if (haveExistingBinding) {
    auto clear = new wxButton(d, wxID_ANY, _("Clear"));
    bs->Add(clear);

    clear->Bind(wxEVT_BUTTON, [=](auto&) {
      d->Close();
      wxQueueEvent(d, new wxCommandEvent(okEVT_DI_CLEAR_BINDING_BUTTON));
    });
  }

  d->SetSizerAndFit(s);

  return d;
}

void okDirectInputSettings::OnBindButton(
  wxCommandEvent& ev,
  int deviceIndex,
  UserAction action) {
  auto button = dynamic_cast<wxButton*>(ev.GetEventObject());
  auto& device = mDevices.at(deviceIndex);

  auto bindings = mDIController->GetBindings();

  // Used to implement a clear button
  auto currentBinding = bindings.end();
  for (auto it = bindings.begin(); it != bindings.end(); ++it) {
    if (it->instanceGuid != device.guidInstance) {
      continue;
    }
    if (it->action != action) {
      continue;
    }
    currentBinding = it;
    break;
  }
  auto d = CreateBindInputDialog(currentBinding != bindings.end());

  auto unhook = scope_guard([=] { mDIController->SetHook(nullptr); });
  mDIController->SetHook([=, &bindings](const DirectInputButtonEvent& be) {
    if (be.instance.guidInstance != device.guidInstance) {
      return;
    }

    // Not using `currentBinding` as we need to clear both the
    // current binding, and any other conflicting bindings
    for (auto it = bindings.begin(); it != bindings.end();) {
      if (it->instanceGuid != device.guidInstance) {
        ++it;
        continue;
      }

      if (it->buttonIndex != be.buttonIndex && it->action != action) {
        ++it;
        continue;
      }

      auto button = this->mDeviceButtons.at(deviceIndex).Get(it->action);
      if (button) {
        button->SetLabel(_("Bind"));
      }
      it = bindings.erase(it);
    }

    button->SetLabel(fmt::format(
      fmt::runtime(_("Button {:d}").ToStdString()), be.buttonIndex + 1));
    bindings.push_back(
      {.instanceGuid = device.guidInstance,
       .instanceName = to_utf8(device.tszInstanceName),
       .buttonIndex = be.buttonIndex,
       .action = action});
    mDIController->SetBindings(bindings);
    this->evSettingsChangedEvent();
    d->Close();
  });

  d->Bind(okEVT_DI_CLEAR_BINDING_BUTTON, [=, &bindings](auto&) {
    bindings.erase(currentBinding);
    auto button = this->mDeviceButtons.at(deviceIndex).Get(action);
    if (button) {
      button->SetLabel(_("Bind"));
    }
    mDIController->SetBindings(bindings);
    this->evSettingsChangedEvent();
  });
  d->ShowModal();
}
