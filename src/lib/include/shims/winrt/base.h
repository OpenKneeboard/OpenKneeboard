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

#include <shims/source_location>

#include <pplawait.h>
#include <ppltasks.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <OpenKneeboard/tracing.hpp>
#pragma warning(push)
#pragma warning(disable : 26820)// Potentially expensive copy operation
#include <winrt/base.h>
#pragma warning(pop)

#include <stop_token>

namespace OpenKneeboard {

// FUTURE - C++?? (maybe?) - add [[nodiscard]]:
// https://github.com/cplusplus/papers/issues/1888
using IAsyncAction = winrt::Windows::Foundation::IAsyncAction;

inline auto random_guid() {
  winrt::guid ret;
  winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID*>(&ret)));
  return ret;
}

inline winrt::Windows::Foundation::IAsyncAction
make_stoppable(std::stop_token token, auto action, std::source_location loc) {
  const auto src = std::format("{}", loc);
  if (token.stop_requested()) {
    co_return;
  }

  try {
    auto op = ([](auto action) -> winrt::Windows::Foundation::IAsyncAction {
      auto tok = co_await winrt::get_cancellation_token();
      tok.enable_propagation();
      co_await action();
    })(action);
    const std::stop_callback callback(token, [&op, &src]() {
      TraceLoggingWrite(
        gTraceProvider,
        "make_stoppable()/cancel",
        TraceLoggingString(src.c_str(), "Source"));
      op.Cancel();
    });
    co_await op;
    co_return;
  } catch (const winrt::hresult_canceled&) {
    TraceLoggingWrite(
      gTraceProvider,
      "make_stoppable()/cancelled",
      TraceLoggingString(src.c_str(), "Source"));
  }
}

inline winrt::Windows::Foundation::IAsyncAction resume_on_signal(
  std::stop_token token,
  HANDLE handle,
  winrt::Windows::Foundation::TimeSpan timeout = {},
  std::source_location loc = std::source_location::current()) {
  return make_stoppable(
    token,
    [handle, timeout]() { return winrt::resume_on_signal(handle, timeout); },
    loc);
}

inline winrt::Windows::Foundation::IAsyncAction resume_after(
  std::stop_token token,
  winrt::Windows::Foundation::TimeSpan timeout,
  std::source_location loc = std::source_location::current()) {
  return make_stoppable(
    token, [timeout]() { return winrt::resume_after(timeout); }, loc);
}

/// Used for intentionally await and discarding `[[nodiscard]]` IAsyncAction
inline winrt::fire_and_forget fire_and_forget(
  winrt::Windows::Foundation::IAsyncAction&& x) {
  co_await x;
}

}// namespace OpenKneeboard

template <class CharT>
struct std::formatter<winrt::hstring, CharT>
  : std::formatter<std::basic_string_view<CharT>, CharT> {
  auto format(const winrt::hstring& value, auto& formatContext) const {
    if constexpr (std::same_as<CharT, wchar_t>) {
      return std::formatter<std::basic_string_view<CharT>, CharT>::format(
        static_cast<std::basic_string_view<CharT>>(value), formatContext);
    } else {
      return std::formatter<std::basic_string_view<CharT>, CharT>::format(
        winrt::to_string(value), formatContext);
    }
  }
};

template <>
struct std::formatter<winrt::guid, wchar_t>
  : std::formatter<winrt::hstring, wchar_t> {
  auto format(const winrt::guid& guid, auto& formatContext) const {
    auto string = winrt::to_hstring(guid);
    return std::formatter<winrt::hstring, wchar_t>::format(
      winrt::to_hstring(guid), formatContext);
  }
};

template <>
struct std::formatter<winrt::guid, char> {
  auto format(const winrt::guid& guid, auto& ctx) const {
    auto string = winrt::to_string(winrt::to_hstring(guid));
    std::string_view view {string};
    if (!mWithBraces) {
      view.remove_prefix(1);
      view.remove_suffix(1);
    }
    return std::ranges::copy(view, ctx.out()).out;
  }

  constexpr auto parse(auto& ctx) {
    auto it = ctx.begin();
    if (it == ctx.end()) {
      return it;
    }

    constexpr std::string_view nobraces = "nobraces";

    if (std::string_view(it, ctx.end()).starts_with(nobraces)) {
      mWithBraces = false;
      it += nobraces.size();
    }

    if (it != ctx.end() && *it != '}') {
      throw std::format_error("Invalid format args for winrt::guid");
    }

    return it;
  }

 private:
  bool mWithBraces = true;
};