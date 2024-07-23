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

#include <OpenKneeboard/DirectInputListener.hpp>

#include <array>

namespace OpenKneeboard {

class DirectInputKeyboardListener final : public DirectInputListener {
 public:
  DirectInputKeyboardListener(
    const std::stop_token&,
    const winrt::com_ptr<IDirectInput8>& di,
    const std::shared_ptr<DirectInputDevice>& device);
  ~DirectInputKeyboardListener();

 protected:
  virtual void Poll() override;
  virtual void SetDataFormat() noexcept override;
  virtual void OnAcquired() noexcept override;

 private:
  std::array<unsigned char, 256> mState;
};

}// namespace OpenKneeboard
