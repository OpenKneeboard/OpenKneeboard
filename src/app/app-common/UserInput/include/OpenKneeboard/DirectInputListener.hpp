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

#include <OpenKneeboard/Events.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

#include <array>

#include <dinput.h>

namespace OpenKneeboard {

class DirectInputDevice;

class DirectInputListener {
 public:
  virtual ~DirectInputListener();

  /** Run until completion or cancellation.
   *
   * `task<void>` instantly fails, even if the coroutine hasn't
   * finished - so wait for the signal.
   */
  static task<void> Run(
    std::stop_token,
    winrt::com_ptr<IDirectInput8> di,
    std::shared_ptr<DirectInputDevice> device);

 protected:
  DirectInputListener(
    const std::stop_token&,
    const winrt::com_ptr<IDirectInput8>& di,
    const std::shared_ptr<DirectInputDevice>& device);

  virtual void Poll() = 0;
  virtual void SetDataFormat() noexcept = 0;
  virtual void OnAcquired() noexcept = 0;

  template <class T>
  void GetState(DWORD size, T* state) {
    winrt::check_hresult(mDIDevice->GetDeviceState(size, state));
  }

  std::shared_ptr<DirectInputDevice> GetDevice() const;
  winrt::com_ptr<IDirectInputDevice8W> GetDIDevice() const;

 private:
  std::stop_token mStopToken;
  std::shared_ptr<DirectInputDevice> mDevice;
  winrt::com_ptr<IDirectInputDevice8> mDIDevice;
  winrt::handle mEventHandle;
  bool mInitialized = false;

  task<void> Run() noexcept;
  void Initialize();
};

}// namespace OpenKneeboard
