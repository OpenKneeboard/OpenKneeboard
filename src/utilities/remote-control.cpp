// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "simple-remotes.hpp"

#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <windows.h>

#include <magic_args/magic_args.hpp>
#include <magic_args/subcommands.hpp>

using namespace OpenKneeboard;

struct RootInfo {
  using subcommands = SimpleRemotes::subcommands;

  static int main(
    const std::expected<
      int,
      magic_args::subcommands_winmain_unexpected_t<subcommands>>& exitCode,
    HINSTANCE,
    [[maybe_unused]] const int nCmdShow) {
    if (exitCode) [[likely]] {
      return *exitCode;
    }

    using OpenKneeboard::Win32;
    const auto message = Win32::UTF8::to_wide(exitCode.error().output);
    const auto title =
      std::filesystem::path {Win32::Wide::or_throw::GetModuleFileName()}
        .stem()
        .wstring();

    MessageBoxW(
      nullptr,
      message->c_str(),
      title.c_str(),
      MB_OK | (is_error(exitCode.error()) ? MB_ICONERROR : MB_ICONINFORMATION));

    return is_error(exitCode.error()) ? EXIT_FAILURE : EXIT_SUCCESS;
  }

  template <class T, auto Name>
  static consteval auto normalize_subcommand_name() {
    constexpr std::string_view prefix {"OpenKneeboard-RemoteControl-"};
    constexpr std::string_view suffix {magic_enum::enum_name<T::action>()};
    std::array<char, prefix.size() + suffix.size()> ret;
    auto begin = std::ranges::copy(prefix, ret.begin()).out;
    std::ranges::copy(suffix, begin);
    return ret;
  }
};

MAGIC_ARGS_MULTICALL_WINMAIN(RootInfo)
