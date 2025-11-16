// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/KneeboardState.hpp>

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Xaml.Data.h>

namespace OpenKneeboard {

struct WithPropertyChangedEvent {
  winrt::event_token PropertyChanged(
    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const&
      handler);
  void PropertyChanged(winrt::event_token const& token) noexcept;

 protected:
  winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler>
    mPropertyChangedEvent;

  void EmitPropertyChangedEvent(this auto& self, auto property) {
    self.mPropertyChangedEvent(
      self,
      winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(property));
  }
};

template <class T>
struct WithPropertyChangedEventOnProfileChange
  : virtual WithPropertyChangedEvent,
    virtual EventReceiver {
  WithPropertyChangedEventOnProfileChange() {
    mProfileChangedEvent = AddEventListener(
      gKneeboard.lock()->evCurrentProfileChangedEvent,
      [&self = *static_cast<T*>(this)]() { self.EmitPropertyChangedEvent(L""); }
        | bind_winrt_context(mUIThread));
  }

  ~WithPropertyChangedEventOnProfileChange() {
    this->RemoveEventListener(mProfileChangedEvent);
  }

 private:
  winrt::apartment_context mUIThread;
  EventHandlerToken mProfileChangedEvent {};
};

}// namespace OpenKneeboard
