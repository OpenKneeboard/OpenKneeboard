#include "okDirectInputPageController.h"

#include <winrt/base.h>
#include <wx/listctrl.h>

#include "OpenKneeboard/dprint.h"

using namespace OpenKneeboard;

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {
winrt::com_ptr<IDirectInput8> GetDI8() {
  static winrt::com_ptr<IDirectInput8> inst;
  if (!inst) {
    DirectInput8Create(
      GetModuleHandle(NULL),
      DIRECTINPUT_VERSION,
      IID_IDirectInput8,
      inst.put_void(),
      NULL);
  }
  return inst;
}

using DeviceInstances = std::vector<DIDEVICEINSTANCE>;

BOOL CALLBACK EnumDeviceCallback(LPCDIDEVICEINSTANCE inst, LPVOID ctx) {
  auto& devices = *reinterpret_cast<DeviceInstances*>(ctx);
  devices.push_back(*inst);
  return DIENUM_CONTINUE;
}

DeviceInstances EnumDevices() {
  auto di = GetDI8();
  DeviceInstances ret;
  di->EnumDevices(
    DI8DEVCLASS_GAMECTRL, &EnumDeviceCallback, &ret, DIEDFL_ATTACHEDONLY);
  return ret;
}

struct DIButtonEvent {
  bool Valid = false;
  DIDEVICEINSTANCE Instance;
  uint8_t ButtonID;
  bool ButtonState;

  operator bool() const {
  return Valid;
}
};

class DIButtonListener final : public wxEvtHandler {
 private:
  struct DeviceInfo {
    DIDEVICEINSTANCE Instance;
    winrt::com_ptr<IDirectInputDevice8> Device;
    DIJOYSTATE2 State = {};
    HANDLE EventHandle = INVALID_HANDLE_VALUE;

    ~DeviceInfo() {
      CloseHandle(EventHandle);
    }
  };
  std::vector<DeviceInfo> mDevices;
  HANDLE mCancelHandle = INVALID_HANDLE_VALUE;

 public:
  DIButtonListener(const DeviceInstances& instances) {
    auto di = GetDI8();
    for (const auto& it: instances) {
      winrt::com_ptr<IDirectInputDevice8> device;
      di->CreateDevice(it.guidInstance, device.put(), NULL);
      if (!device) {
        continue;
      }
      device->SetDataFormat(&c_dfDIJoystick2);
      device->Acquire();

      DIJOYSTATE2 state;
      device->GetDeviceState(sizeof(state), &state);

      HANDLE event {CreateEvent(nullptr, false, false, nullptr)};
      if (!event) {
        continue;
      }

      device->SetEventNotification(event);

      mDevices.push_back({it, device, state, event});
    }
    mCancelHandle = CreateEvent(nullptr, false, false, nullptr);
  }

  ~DIButtonListener() {
    CloseHandle(mCancelHandle);
  }

  void Cancel() {
    SetEvent(mCancelHandle);
  }

  DIButtonEvent Poll() {
    std::vector<HANDLE> handles;
    for (const auto& it: mDevices) {
      handles.push_back(it.EventHandle);
    }
    handles.push_back(mCancelHandle);

    auto result
      = WaitForMultipleObjects(handles.size(), handles.data(), false, 100);
    auto idx = result - WAIT_OBJECT_0;
    if (idx < 0 || idx >= (handles.size() - 1)) {
      return {};
    }

    auto& device = mDevices.at(idx);
    auto oldState = device.State;
    DIJOYSTATE2 newState;
    device.Device->GetDeviceState(sizeof(newState), &newState);
    for (uint8_t i = 0; i < 128; ++i) {
      if (oldState.rgbButtons[i] != newState.rgbButtons[i]) {
        device.State = newState;
        return {
          true,
          device.Instance,
          static_cast<uint8_t>(i + 1),
          (bool)(newState.rgbButtons[i] & (1 << 7))};
      }
    }
  }
};

}// namespace

wxDEFINE_EVENT(okEVT_DI_BUTTON_EVENT, wxThreadEvent);

class okDirectInputThread final : public wxThread {
 private:
  wxEvtHandler* mReceiver;

 public:
  okDirectInputThread(wxEvtHandler* receiver)
    : wxThread(wxTHREAD_JOINABLE), mReceiver(receiver) {
  }

 protected:
  virtual ExitCode Entry() override {
    auto listener = DIButtonListener(EnumDevices());
    while (!this->TestDestroy()) {
      DIButtonEvent bi = listener.Poll();
      if (!bi) {
        continue;
      }
      wxThreadEvent ev(okEVT_DI_BUTTON_EVENT);
      ev.SetPayload(bi);
      wxQueueEvent(mReceiver, ev.Clone());
    }
  }
};

class okDirectInputPageSettings final : public wxPanel {
 private:
  DeviceInstances mDevices;
  wxListView* mBindings;

 public:
  okDirectInputPageSettings(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    auto sizer = new wxBoxSizer(wxVERTICAL);

    mBindings = new wxListView(this, wxID_ANY);
    sizer->Add(mBindings);
    mBindings->AppendColumn(_("Device"));
    mBindings->AppendColumn(_("Previous Tab"));
    mBindings->AppendColumn(_("Next Tab"));
    mBindings->AppendColumn(_("Previous Page"));
    mBindings->AppendColumn(_("Next Page"));
    mBindings->Bind(wxEVT_LIST_ITEM_ACTIVATED, &okDirectInputPageSettings::OnItemActivated, this);

    mDevices = EnumDevices();
    for (int i = 0; i < mDevices.size(); ++i) {
      const auto& device = mDevices.at(i);
      mBindings->InsertItem(i, device.tszInstanceName);
    }

    this->SetSizerAndFit(sizer);
  }

  void OnItemActivated(const wxListEvent& ev) {
    dprintf("Item activated: row {} column {}", ev.GetItem(), ev.GetColumn());
  }
};

wxString okDirectInputPageController::GetTitle() const {
  return _("DirectInput");
}

wxWindow* okDirectInputPageController::GetSettingsUI(wxWindow* parent) const {
  return new okDirectInputPageSettings(parent);
}
