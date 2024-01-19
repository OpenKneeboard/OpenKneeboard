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

#include <OpenKneeboard/dprint.h>

#include <concepts>
#include <format>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace OpenKneeboard {

template <auto InState, auto OutState>
constexpr bool is_valid_state_transition_v = false;

namespace ADL {
template <class State>
constexpr auto formattable_state(State state) noexcept {
  return static_cast<std::underlying_type_t<State>>(state);
}
}// namespace ADL

template <class State>
class StateMachine final {
 public:
  StateMachine() = delete;
  constexpr StateMachine(State initialState) : mState(initialState) {
  }

  template <State in, State out>
    requires is_valid_state_transition_v<in, out>
  constexpr void Transition() {
    if (mState != in) [[unlikely]] {
      using namespace ADL;
      const auto message = std::format(
        "Unexpected state {}; expected {} -> {}",
        formattable_state(mState),
        formattable_state(in),
        formattable_state(out));
      dprint(message);
      OPENKNEEBOARD_BREAK;
      throw std::logic_error(message);
    }
    mState = out;
  }

  State Get() const noexcept {
    return mState;
  }

  StateMachine(const StateMachine&) = delete;
  StateMachine(StateMachine&&) = delete;
  StateMachine& operator=(const StateMachine&) = delete;
  StateMachine& operator=(StateMachine&&) = delete;

 private:
  State mState;
};

template <class TStateMachine, auto pre, auto state, auto post>
class ScopedStateTransitions final {
 public:
  ScopedStateTransitions() = delete;
  ScopedStateTransitions(TStateMachine* impl) : mImpl(impl) {
    mImpl->Transition<pre, state>();
  }

  ~ScopedStateTransitions() {
    mImpl->Transition<state, post>();
  }

  ScopedStateTransitions(ScopedStateTransitions&&) = delete;
  ScopedStateTransitions(const ScopedStateTransitions&) = delete;
  ScopedStateTransitions& operator=(const ScopedStateTransitions&) = delete;
  ScopedStateTransitions& operator=(ScopedStateTransitions&&) = delete;

 private:
  TStateMachine* mImpl {nullptr};
};

namespace Detail {

template <class T>
concept raw_pointer = std::is_pointer_v<std::remove_cvref_t<T>>;

template <class T>
auto get_pointer(const std::shared_ptr<T>& p) {
  return p.get();
}

template <class T>
auto get_pointer(const std::unique_ptr<T>& p) {
  return p.get();
}

template <raw_pointer T>
auto get_pointer(T p) {
  return p;
}

template <class T>
concept any_pointer = raw_pointer<decltype(get_pointer(std::declval<T>()))>;

static_assert(std::same_as<int*, decltype(get_pointer(std::declval<int*>()))>);
static_assert(
  std::same_as<
    int*,
    decltype(get_pointer(std::declval<const std::unique_ptr<int>&>()))>);
static_assert(
  std::same_as<
    int*,
    decltype(get_pointer(std::declval<const std::shared_ptr<int>&>()))>);

}// namespace Detail

template <auto pre, auto state, auto post>
auto make_scoped_state_transitions(Detail::any_pointer auto&& stateMachine) {
  auto smPtr
    = Detail::get_pointer(std::forward<decltype(stateMachine)>(stateMachine));
  static_assert(Detail::raw_pointer<decltype(smPtr)>);
  static_assert(!Detail::any_pointer<decltype(*smPtr)>);
  using TStateMachine = std::remove_cvref_t<decltype(*smPtr)>;
  return ScopedStateTransitions<TStateMachine, pre, state, post>(smPtr);
}

template <class State>
concept lockable_state = requires() {
  { State::Unlocked } -> std::convertible_to<State>;
  { State::TryLock } -> std::convertible_to<State>;
  { State::Locked } -> std::convertible_to<State>;
}
// clang-format off
    && is_valid_state_transition_v<State::Unlocked, State::TryLock>
    && is_valid_state_transition_v<State::TryLock, State::Locked>
    && is_valid_state_transition_v<State::TryLock, State::Unlocked>
    && is_valid_state_transition_v<State::Locked, State::Unlocked>
    && !is_valid_state_transition_v<State::Locked, State::TryLock>;
// clang-format on

enum class LockState {
  Unlocked,
  TryLock,
  Locked,
};

#define OPENKNEEBOARD_DECLARE_STATE_TRANSITION(IN, OUT) \
  template <> \
  constexpr bool is_valid_state_transition_v<IN, OUT> = true;
#define OPENKNEEBOARD_DECLARE_LOCKABLE_STATE_TRANSITIONS(STATES) \
  OPENKNEEBOARD_DECLARE_STATE_TRANSITION(STATES::Unlocked, STATES::TryLock) \
  OPENKNEEBOARD_DECLARE_STATE_TRANSITION(STATES::TryLock, STATES::Unlocked) \
  OPENKNEEBOARD_DECLARE_STATE_TRANSITION(STATES::TryLock, STATES::Locked) \
  OPENKNEEBOARD_DECLARE_STATE_TRANSITION(STATES::Locked, STATES::Unlocked)
OPENKNEEBOARD_DECLARE_LOCKABLE_STATE_TRANSITIONS(LockState);
static_assert(lockable_state<LockState>);

}// namespace OpenKneeboard