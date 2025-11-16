// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
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
  std::expected<void, HRESULT> Poll() override;
  void SetDataFormat() noexcept override;
  void OnAcquired() noexcept override;

 private:
  std::array<unsigned char, 256> mState;
};

}// namespace OpenKneeboard
