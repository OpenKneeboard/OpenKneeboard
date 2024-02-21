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
// <Unknwn.h> must be included before <winrt/base.h> for com_ptr::as<> to work
// correctly
#include <Unknwn.h>
// clang-format on

#include <pplawait.h>
#include <ppltasks.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/base.h>

#include <stop_token>

namespace OpenKneeboard {

auto discard_winrt_event_args(auto func) {
  return [func](const winrt::Windows::Foundation::IInspectable&, const auto&) {
    return func();
  };
}

inline auto random_guid() {
  winrt::guid ret;
  CoCreateGuid(reinterpret_cast<GUID*>(&ret));
  return ret;
}

inline winrt::Windows::Foundation::IAsyncAction make_stoppable(
  const std::stop_token& token,
  auto action) {
  if (token.stop_requested()) {
    co_return;
  }

  try {
    auto op = ([&action]() -> winrt::Windows::Foundation::IAsyncAction {
      auto winrtToken = co_await winrt::get_cancellation_token();
      winrtToken.enable_propagation();
      co_await action();
    })();
    const std::stop_callback callback(token, [&op]() { op.Cancel(); });
    co_await op;
  } catch (const winrt::hresult_canceled&) {
  } catch (const winrt::hresult_error& e) {
    auto x = to_string(e.message());
    __debugbreak();
  } catch (const std::exception& e) {
    auto x = e.what();
    __debugbreak();
  }
}

inline winrt::Windows::Foundation::IAsyncAction resume_on_signal(
  const std::stop_token& token,
  void* handle,
  winrt::Windows::Foundation::TimeSpan timeout = {}) {
  co_await make_stoppable(
    token, [handle, timeout]() -> winrt::Windows::Foundation::IAsyncAction {
      co_await winrt::resume_on_signal(handle, timeout);
    });
}

inline winrt::Windows::Foundation::IAsyncAction resume_after(
  const std::stop_token& token,
  winrt::Windows::Foundation::TimeSpan timeout) {
  co_await make_stoppable(
    token, [timeout]() -> winrt::Windows::Foundation::IAsyncAction {
      co_await winrt::resume_after(timeout);
    });
}

}// namespace OpenKneeboard
