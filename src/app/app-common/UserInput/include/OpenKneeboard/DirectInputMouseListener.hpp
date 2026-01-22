// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DirectInputListener.hpp>

namespace OpenKneeboard {

class DirectInputMouseListener final : public DirectInputListener {
 public:
  DirectInputMouseListener(
    const std::stop_token&,
    const winrt::com_ptr<IDirectInput8>& di,
    const std::shared_ptr<DirectInputDevice>& device);
  ~DirectInputMouseListener();

 protected:
  std::expected<void, HRESULT> Poll() override;
  void SetDataFormat() noexcept override;
  void OnAcquired() noexcept override;

 private:
  DIMOUSESTATE2 mState;
};

}// namespace OpenKneeboard
