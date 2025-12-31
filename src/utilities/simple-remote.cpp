// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/UserAction.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <windows.h>

#include <magic_args/magic_args.hpp>
#include <magic_args/subcommands.hpp>

#include <filesystem>

using namespace OpenKneeboard;

template <UserAction TAction>
struct SimpleRemote {
  static constexpr auto action = TAction;

  struct arguments_type {
    magic_args::optional_positional_argument<std::size_t> mCount {
      .storage = 1,// default
      .help = "Number of times to perform the action",
    };
  };

  static int main(const arguments_type& args) {
    static const std::string ActionString {magic_enum::enum_name(TAction)};

    const auto count = *args.mCount;

    dprint("Remote invoked: {} (count: {})", ActionString, count);

    switch (count) {
      case 0:
        return EXIT_SUCCESS;
      case 1:
        APIEvent {APIEvent::EVT_REMOTE_USER_ACTION, ActionString}.Send();
        return EXIT_SUCCESS;
      default: {
        const auto single = nlohmann::json::array(
          {APIEvent::EVT_REMOTE_USER_ACTION, ActionString});

        auto payload = nlohmann::json::array();
        for (std::size_t i = 0; i < count; ++i) {
          payload.push_back(single);
        }
        APIEvent {APIEvent::EVT_MULTI_EVENT, payload.dump()}.Send();
        return EXIT_SUCCESS;
      }
    }
  }
};

namespace SimpleRemotes {
template <UserAction... Actions>
using make = magic_args::invocable_subcommands_list<SimpleRemote<Actions>...>;

using enum UserAction;
using subcommands = make<
  CYCLE_ACTIVE_VIEW,
  DECREASE_BRIGHTNESS,
  DISABLE_TINT,
  ENABLE_TINT,
  HIDE,
  INCREASE_BRIGHTNESS,
  NEXT_BOOKMARK,
  NEXT_PAGE,
  NEXT_PROFILE,
  NEXT_TAB,
  PREVIOUS_BOOKMARK,
  PREVIOUS_PAGE,
  PREVIOUS_PROFILE,
  PREVIOUS_TAB,
  RECENTER_VR,
  REPAINT_NOW,
  SHOW,
  SWAP_FIRST_TWO_VIEWS,
  TOGGLE_BOOKMARK,
  TOGGLE_FORCE_ZOOM,
  TOGGLE_TINT,
  TOGGLE_VISIBILITY>;
};// namespace SimpleRemotes

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
    const auto title
      = std::filesystem::path {Win32::Wide::or_throw::GetModuleFileName()}
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