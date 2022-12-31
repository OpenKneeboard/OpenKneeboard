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

// clang-format off
#include "pch.h"
// clang-format on

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/weak_wrap.h>
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
};

template <class T>
struct WithPropertyChangedEventOnProfileChange
  : virtual WithPropertyChangedEvent,
    virtual EventReceiver {
  WithPropertyChangedEventOnProfileChange() {
    mProfileChangedEvent = AddEventListener(
      gKneeboard->evCurrentProfileChangedEvent,
      weak_wrap(
        [](auto self) -> winrt::fire_and_forget {
          co_await static_cast<WithPropertyChangedEventOnProfileChange<T>*>(
            self.get())
            ->mUIThread;
          self->mPropertyChangedEvent(
            *self,
            winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L""));
        },
        static_cast<T*>(this)));
  }

  ~WithPropertyChangedEventOnProfileChange() {
    this->RemoveEventListener(mProfileChangedEvent);
  }

 private:
  winrt::apartment_context mUIThread;
  EventHandlerToken mProfileChangedEvent {};
};

}// namespace OpenKneeboard
