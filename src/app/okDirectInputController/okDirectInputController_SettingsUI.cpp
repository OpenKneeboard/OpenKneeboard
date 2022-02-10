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
#include "okDirectInputController_SettingsUI.h"

#include <OpenKneeboard/utf8.h>
#include <fmt/format.h>
#include <wx/gbsizer.h>
#include <wx/scopeguard.h>

#include "GetDirectInputDevices.h"
#include "okDirectInputController_DIButtonEvent.h"
#include "okDirectInputController_SharedState.h"
#include "okEvents.h"

using namespace OpenKneeboard;

wxDEFINE_EVENT(okEVT_DI_CLEAR_BINDING_BUTTON, wxCommandEvent);

struct okDirectInputController::SettingsUI::DeviceButtons {
  wxButton* previousTab = nullptr;
  wxButton* nextTab = nullptr;
  wxButton* previousPage = nullptr;
  wxButton* nextPage = nullptr;
  wxButton* toggleVisibility = nullptr;

  wxButton* Get(const wxEventTypeTag<wxCommandEvent>& evt) {
    if (evt == okEVT_PREVIOUS_TAB) {
      return previousTab;
    }
    if (evt == okEVT_NEXT_TAB) {
      return nextTab;
    }
    if (evt == okEVT_PREVIOUS_PAGE) {
      return previousPage;
    }
    if (evt == okEVT_NEXT_PAGE) {
      return nextPage;
    }
    if (evt == okEVT_TOGGLE_VISIBILITY) {
      return toggleVisibility;
    }
    return nullptr;
  }
};

wxButton* okDirectInputController::SettingsUI::CreateBindButton(
  wxWindow* parent,
  int deviceIndex,
  const wxEventTypeTag<wxCommandEvent>& eventType) {
  const auto& device = mDevices.at(deviceIndex);
  auto label = _("Bind");
  for (auto binding: mControllerState->bindings) {
    if (
      binding.instanceGuid == device.guidInstance
      && binding.eventType == eventType) {
      label = fmt::format(
        fmt::runtime(_("Button {}").ToStdString()), binding.buttonIndex + 1);
      break;
    }
  }
  auto button = new wxButton(parent, wxID_ANY, label);
  button->Bind(wxEVT_BUTTON, [=](auto& ev) {
    this->OnBindButton(ev, deviceIndex, eventType);
  });
  return button;
}

okDirectInputController::SettingsUI::SettingsUI(
  wxWindow* parent,
  const std::shared_ptr<SharedState>& controllerState)
  : wxPanel(parent, wxID_ANY), mControllerState(controllerState) {
  this->SetLabel(_("DirectInput"));
  mDevices = GetDirectInputDevices(controllerState->di8);

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

    auto toggleVisibility = CreateBindButton(panel, i, okEVT_TOGGLE_VISIBILITY);
    grid->Add(toggleVisibility, wxGBPosition(row, 1));

    auto previousTab = CreateBindButton(panel, i, okEVT_PREVIOUS_TAB);
    grid->Add(previousTab, wxGBPosition(row, 2));

    auto nextTab = CreateBindButton(panel, i, okEVT_NEXT_TAB);
    grid->Add(nextTab, wxGBPosition(row, 3));

    auto previousPage = CreateBindButton(panel, i, okEVT_PREVIOUS_PAGE);
    grid->Add(previousPage, wxGBPosition(row, 4));

    auto nextPage = CreateBindButton(panel, i, okEVT_NEXT_PAGE);
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

okDirectInputController::SettingsUI::~SettingsUI() {
}

wxDialog* okDirectInputController::SettingsUI::CreateBindInputDialog(
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

void okDirectInputController::SettingsUI::OnBindButton(
  wxCommandEvent& ev,
  int deviceIndex,
  const wxEventTypeTag<wxCommandEvent>& eventType) {
  auto button = dynamic_cast<wxButton*>(ev.GetEventObject());
  auto& device = mDevices.at(deviceIndex);

  auto& bindings = this->mControllerState->bindings;

  // Used to implement a clear button
  auto currentBinding = bindings.end();
  ;
  for (auto it = bindings.begin(); it != bindings.end(); ++it) {
    if (it->instanceGuid != device.guidInstance) {
      continue;
    }
    if (it->eventType != eventType) {
      continue;
    }
    currentBinding = it;
    break;
  }
  auto d = CreateBindInputDialog(currentBinding != bindings.end());

  wxON_BLOCK_EXIT_SET(mControllerState->hook, nullptr);
  mControllerState->hook = this;

  auto f = [=, &bindings](wxThreadEvent& ev) {
    auto be = ev.GetPayload<DIButtonEvent>();
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

      if (it->buttonIndex != be.buttonIndex && it->eventType != eventType) {
        ++it;
        continue;
      }

      auto button = this->mDeviceButtons.at(deviceIndex).Get(it->eventType);
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
       .eventType = eventType});
    wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED, wxID_ANY));
    d->Close();
  };

  this->Bind(okEVT_DI_BUTTON, f);
  d->Bind(okEVT_DI_CLEAR_BINDING_BUTTON, [=, &bindings](auto&) {
    bindings.erase(currentBinding);
    auto button = this->mDeviceButtons.at(deviceIndex).Get(eventType);
    if (button) {
      button->SetLabel(_("Bind"));
    }
    wxQueueEvent(this, new wxCommandEvent(okEVT_SETTINGS_CHANGED, wxID_ANY));
  });
  d->ShowModal();
  this->Unbind(okEVT_DI_BUTTON, f);
}
