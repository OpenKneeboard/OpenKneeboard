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
#include <dinput.h>
#include <shims/winrt.h>

#include <stop_token>

namespace OpenKneeboard {

class DirectInputDevice;

class DirectInputListener final {
 public:
  DirectInputListener(
    const winrt::com_ptr<IDirectInput8>& di,
    const std::vector<std::shared_ptr<DirectInputDevice>>& devices);
  ~DirectInputListener();

  void Run(std::stop_token);

 private:
  struct DeviceInfo;
  std::vector<DeviceInfo> mDevices;
};

}// namespace OpenKneeboard
