#include "okDirectInputController_SettingsUI.h"

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
  wxButton* PreviousTab = nullptr;
  wxButton* NextTab = nullptr;
  wxButton* PreviousPage = nullptr;
  wxButton* NextPage = nullptr;
  wxButton* Get(const wxEventTypeTag<wxCommandEvent>& evt) {
    if (evt == okEVT_PREVIOUS_TAB) {
      return PreviousTab;
    }
    if (evt == okEVT_NEXT_TAB) {
      return NextTab;
    }
    if (evt == okEVT_PREVIOUS_PAGE) {
      return PreviousPage;
    }
    if (evt == okEVT_NEXT_PAGE) {
      return NextPage;
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
  for (auto binding: mControllerState->Bindings) {
    if (
      binding.InstanceGuid == device.guidInstance
      && binding.EventType == eventType) {
      label
        = fmt::format(_("Button {}").ToStdString(), binding.ButtonIndex + 1);
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
  mDevices = GetDirectInputDevices(controllerState->DI8);

  auto sizer = new wxBoxSizer(wxVERTICAL);

  auto panel = new wxPanel(this, wxID_ANY);
  sizer->Add(panel, 0, wxEXPAND);

  auto grid = new wxGridBagSizer(5, 5);
  grid->AddGrowableCol(0);

  auto bold = GetFont().MakeBold();
  for (const auto& column:
       {_("Device"),
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

    auto previousTab = CreateBindButton(panel, i, okEVT_PREVIOUS_TAB);
    grid->Add(previousTab, wxGBPosition(row, 1));

    auto nextTab = CreateBindButton(panel, i, okEVT_NEXT_TAB);
    grid->Add(nextTab, wxGBPosition(row, 2));

    auto previousPage = CreateBindButton(panel, i, okEVT_PREVIOUS_PAGE);
    grid->Add(previousPage, wxGBPosition(row, 3));

    auto nextPage = CreateBindButton(panel, i, okEVT_NEXT_PAGE);
    grid->Add(nextPage, wxGBPosition(row, 4));

    mDeviceButtons.push_back({
      .PreviousTab = previousTab,
      .NextTab = nextTab,
      .PreviousPage = previousPage,
      .NextPage = nextPage,
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

  auto& bindings = this->mControllerState->Bindings;

  // Used to implement a clear button
  auto currentBinding = bindings.end();
  ;
  for (auto it = bindings.begin(); it != bindings.end(); ++it) {
    if (it->InstanceGuid != device.guidInstance) {
      continue;
    }
    if (it->EventType != eventType) {
      continue;
    }
    currentBinding = it;
    break;
  }
  auto d = CreateBindInputDialog(currentBinding != bindings.end());

  wxON_BLOCK_EXIT_SET(mControllerState->Hook, nullptr);
  mControllerState->Hook = this;

  auto f = [=, &bindings](wxThreadEvent& ev) {
    auto be = ev.GetPayload<DIButtonEvent>();
    if (be.Instance.guidInstance != device.guidInstance) {
      return;
    }

    // Not using `currentBinding` as we need to clear both the
    // current binding, and any other conflicting bindings
    for (auto it = bindings.begin(); it != bindings.end();) {
      if (it->InstanceGuid != device.guidInstance) {
        ++it;
        continue;
      }

      if (it->ButtonIndex != be.ButtonIndex && it->EventType != eventType) {
        ++it;
        continue;
      }

      auto button = this->mDeviceButtons.at(deviceIndex).Get(it->EventType);
      if (button) {
        button->SetLabel(_("Bind"));
      }
      it = bindings.erase(it);
    }

    button->SetLabel(
      fmt::format(_("Button {:d}").ToStdString(), be.ButtonIndex + 1));
    bindings.push_back(
      {.InstanceGuid = device.guidInstance,
       .InstanceName = wxString(device.tszInstanceName).ToStdString(),
       .ButtonIndex = be.ButtonIndex,
       .EventType = eventType});
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
