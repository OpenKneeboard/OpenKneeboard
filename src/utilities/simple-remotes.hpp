// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/UserAction.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <magic_args/magic_args.hpp>
#include <magic_args/subcommands.hpp>
#include <magic_enum/magic_enum.hpp>

namespace OpenKneeboard::SimpleRemotes {
using namespace OpenKneeboard;

struct Args {
  magic_args::optional_positional_argument<std::size_t> mCount {
    .storage = 1,// default
    .help = "Number of times to perform the action",
  };
};

// Non-template version so we don't end up with N nearly-identical
// functions in the binary
[[nodiscard]]
int main(UserAction, const Args&);

// Template version as we need a unique type for each subcommand
template <UserAction TAction>
struct SimpleRemote {
  static constexpr auto action = TAction;

  using arguments_type = Args;

  static int main(const arguments_type& args) {
    return SimpleRemotes::main(action, args);
  }
};

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
};// namespace OpenKneeboard::SimpleRemotes