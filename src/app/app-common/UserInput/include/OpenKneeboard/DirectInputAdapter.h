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
#pragma once

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/UserAction.h>
#include <shims/winrt/base.h>
#include <winrt/Windows.Foundation.h>

#include <memory>
#include <nlohmann/json.hpp>

struct IDirectInput8W;

namespace OpenKneeboard {

class DirectInputDevice;
class DirectInputListener;
class UserInputButtonBinding;
class UserInputDevice;

class DirectInputAdapter final : private OpenKneeboard::EventReceiver {
 public:
  DirectInputAdapter() = delete;
  DirectInputAdapter(const nlohmann::json& settings);
  ~DirectInputAdapter();

  nlohmann::json GetSettings() const;
  std::vector<std::shared_ptr<UserInputDevice>> GetDevices() const;

  Event<UserAction> evUserActionEvent;
  Event<> evSettingsChangedEvent;

 private:
  winrt::com_ptr<IDirectInput8W> mDI8;
  std::vector<std::shared_ptr<DirectInputDevice>> mDevices;

  std::unique_ptr<DirectInputListener> mListener;
  winrt::Windows::Foundation::IAsyncAction mWorker;

  const nlohmann::json mInitialSettings;
};

}// namespace OpenKneeboard
