// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// <Unknwn.h> must be included before <winrt/base.h> for com_ptr::as<> to work
// correctly
#include <Unknwn.h>

#include <pplawait.h>
#include <ppltasks.h>

#include <source_location>

#pragma warning(push)
#pragma warning(disable : 26820)// Potentially expensive copy operation
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
#if __has_include(<wil/cppwinrt.h> )
#include <wil/cppwinrt.h>
#endif
#include <winrt/base.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma warning(pop)

#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/format/source_location.hpp>
#include <OpenKneeboard/tracing.hpp>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <stop_token>

#if __has_include(<winrt/Microsoft.UI.Dispatching.h>)
#include <winrt/Microsoft.UI.Dispatching.h>

#include <functional>
#endif

namespace OpenKneeboard {

#if __has_include(<winrt/Microsoft.UI.Dispatching.h>)
using DispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue;
using DispatcherQueueController =
  winrt::Microsoft::UI::Dispatching::DispatcherQueueController;

template <class... Args>
inline void disown_later(Args&&... args) {
  winrt::check_bool(
    DispatcherQueue::GetForCurrentThread().TryEnqueue(
      std::bind_front([](auto&&...) {}, std::forward<Args>(args)...)));
}
#else
// Let's provide a nice compiler error message; all actions are invalid on
// these
struct requires_Microsoft_UI_Dispatching_from_WinUI3 {
  requires_Microsoft_UI_Dispatching_from_WinUI3() = delete;
};
using DispatcherQueue = requires_Microsoft_UI_Dispatching_from_WinUI3;
using DispatcherQueueController = requires_Microsoft_UI_Dispatching_from_WinUI3;
#endif

inline auto random_guid() {
  winrt::guid ret;
  winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID*>(&ret)));
  return ret;
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

template <class CharT>
struct std::formatter<winrt::hresult, CharT>
  : std::formatter<std::basic_string_view<CharT>, CharT> {
  template <class FormatContext>
  auto format(const winrt::hresult& hresult, FormatContext& fc) const {
    char* message = nullptr;
    FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      hresult,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<char*>(&message),
      0,
      nullptr);
    std::basic_string<CharT> converted;
    if (!message) {
      converted = std::format("{:#010x}", static_cast<uint32_t>(hresult.value));
      OPENKNEEBOARD_BREAK;
    } else {
      converted = std::format(
        "{:#010x} (\"{}\")", static_cast<uint32_t>(hresult.value), message);
    }

    return std::formatter<std::basic_string_view<CharT>, CharT>::format(
      std::basic_string_view<CharT> {converted}, fc);
  }
};

inline winrt::hstring operator""_hs(const char* str, std::size_t len) {
  return winrt::to_hstring(std::string_view {str, len});
}
