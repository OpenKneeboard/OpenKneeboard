// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/DirectInputDevice.hpp>
#include <OpenKneeboard/DirectInputKeyboardListener.hpp>

#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard {

DirectInputKeyboardListener::DirectInputKeyboardListener(
  const std::stop_token& stop,
  const winrt::com_ptr<IDirectInput8>& di,
  const std::shared_ptr<DirectInputDevice>& device)
  : DirectInputListener(stop, di, device) {}

DirectInputKeyboardListener::~DirectInputKeyboardListener() = default;

std::expected<void, HRESULT> DirectInputKeyboardListener::Poll() {
  decltype(mState) newState {};
  {
    const auto pollResult = this->GetState(newState.size(), &newState);
    if (!pollResult.has_value()) {
      return pollResult;
    }
  }
  scope_exit updateState([&]() { mState = newState; });

  auto device = this->GetDevice();
  for (size_t i = 0; i < newState.size(); ++i) {
    if (mState[i] != newState[i]) {
      device->PostButtonStateChange(
        i, static_cast<bool>(newState[i] & (1 << 7)));
    }
  }

  return {};
}

void DirectInputKeyboardListener::SetDataFormat() noexcept {
  this->GetDIDevice()->SetDataFormat(&c_dfDIKeyboard);
}

void DirectInputKeyboardListener::OnAcquired() noexcept {
  const auto result = this->GetState(sizeof(mState), &mState);
  if (!result.has_value()) {
    winrt::throw_hresult(result.error());
  }
}

}// namespace OpenKneeboard
