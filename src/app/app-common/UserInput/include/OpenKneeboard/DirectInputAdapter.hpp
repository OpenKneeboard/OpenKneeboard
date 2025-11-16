// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DirectInputSettings.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/ProcessShutdownBlock.hpp>
#include <OpenKneeboard/UserAction.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <OpenKneeboard/final_release_deleter.hpp>
#include <OpenKneeboard/json_fwd.hpp>

#include <memory>
#include <shared_mutex>
#include <vector>

struct IDirectInput8W;

namespace OpenKneeboard {

class DirectInputDevice;
class UserInputButtonBinding;
class UserInputDevice;

class DirectInputAdapter final
  : private OpenKneeboard::EventReceiver,
    public std::enable_shared_from_this<DirectInputAdapter> {
 public:
  DirectInputAdapter() = delete;
  static std::shared_ptr<DirectInputAdapter> Create(
    HWND mainWindow,
    const DirectInputSettings& settings);
  ~DirectInputAdapter();
  static OpenKneeboard::fire_and_forget final_release(
    std::unique_ptr<DirectInputAdapter>);

  DirectInputSettings GetSettings() const;
  void LoadSettings(const DirectInputSettings& settings);
  std::vector<std::shared_ptr<UserInputDevice>> GetDevices() const;

  Event<UserAction> evUserActionEvent;
  Event<> evSettingsChangedEvent;
  Event<> evAttachedControllersChangedEvent;

 private:
  ProcessShutdownBlock mShutdownBlock;

  DirectInputAdapter(HWND mainWindow, const DirectInputSettings& settings);
  task<void> ReleaseDevices();
  OpenKneeboard::fire_and_forget Reload();
  OpenKneeboard::fire_and_forget UpdateDevices();

  bool mShuttingDown = false;

  HWND mWindow;
  UINT_PTR mID;
  static UINT_PTR gNextID;

  winrt::com_ptr<IDirectInput8W> mDI8;

  struct DeviceState {
    std::shared_ptr<DirectInputDevice> mDevice;
    task<void> mListener;
    std::stop_source mStop;
  };
  mutable std::shared_mutex mDevicesMutex;
  std::unordered_map<std::string, DeviceState> mDevices;

  mutable DirectInputSettings mSettings;

  static LRESULT SubclassProc(
    HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData);
};

}// namespace OpenKneeboard
