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

#include <array>
#include <atomic>
#include <concepts>
#include <format>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace OpenKneeboard {

namespace ADL {
template <class State>
constexpr auto formattable_state(State state) noexcept {
  return std::to_underlying(state);
}
}// namespace ADL

template <class State>
  requires std::is_enum_v<State>
struct Transition final {
  Transition() = delete;
  consteval Transition(State in, State out) : mIn(in), mOut(out) {
  }

  const State mIn;
  const State mOut;
};

template <class T, size_t N>
using Transitions = std::array<Transition<T>, N>;

template <
  class State,
  class StateContainer,
  State InitialState,
  size_t TransitionCount,
  std::array<Transition<State>, TransitionCount> Transitions>
  requires(TransitionCount >= 1) && std::is_enum_v<State>
class StateMachineBase {
 public:
  using Values = State;

  constexpr StateMachineBase() {
  }

  constexpr State Get() const noexcept {
    return mState;
  }

  template <State T>
  void Assert(
    const std::source_location& caller = std::source_location::current()) {
    if (mState == T) [[likely]] {
      return;
    }
    OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
      caller,
      "Expected state `{}`, but state is `{}`",
      std::to_underlying(T),
      std::to_underlying(this->Get()));
  }

  StateMachineBase(const StateMachineBase&) = delete;
  StateMachineBase(StateMachineBase&&) = delete;
  StateMachineBase& operator=(const StateMachineBase&) = delete;
  StateMachineBase& operator=(StateMachineBase&&) = delete;

  template <State in, State out>
  static consteval bool IsValidTransition() {
    for (const auto& it: Transitions) {
      if (it.mIn == in && it.mOut == out) {
        return true;
      }
    }
    return false;
  }

 protected:
  StateContainer mState {InitialState};
};

template <class State, State InitialState, auto Transitions>
class StateMachine final : public StateMachineBase<
                             State,
                             State,
                             InitialState,
                             Transitions.size(),
                             Transitions> {
 public:
  template <State in, State out>
  constexpr void Transition(
    const std::source_location& loc = std::source_location::current()) {
    static_assert(this->template IsValidTransition<in, out>());
    if (this->mState != in) [[unlikely]] {
      using namespace ADL;
      OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
        loc,
        "Unexpected state `{}`; expected (`{}` -> `{}`)",
        formattable_state(this->mState),
        formattable_state(in),
        formattable_state(out));
    }
    this->mState = out;
  }
};

template <class State, State InitialState, auto Transitions>
class AtomicStateMachine final : public StateMachineBase<
                                   State,
                                   std::atomic<State>,
                                   InitialState,
                                   Transitions.size(),
                                   Transitions> {
 public:
  template <State in, State out>
  constexpr void Transition(
    const std::source_location& loc = std::source_location::current()) {
    static_assert(this->template IsValidTransition<in, out>());
    auto current = in;
    if (!this->mState.compare_exchange_strong(current, out)) [[unlikely]] {
      using namespace ADL;
      OPENKNEEBOARD_LOG_SOURCE_LOCATION_AND_FATAL(
        loc,
        "Unexpected state `{}`; expected (`{}` -> `{}`)",
        formattable_state(current),
        formattable_state(in),
        formattable_state(out));
    }
  }
};

template <class TStateMachine, auto pre, auto state, auto post>
class ScopedStateTransitions final {
 public:
  ScopedStateTransitions() = delete;
  constexpr ScopedStateTransitions(
    TStateMachine* impl,
    const std::source_location& loc = std::source_location::current())
    : mImpl(impl), mSourceLocation(loc) {
    mImpl->template Transition<pre, state>(loc);
  }

  ~ScopedStateTransitions() {
    mImpl->template Transition<state, post>(mSourceLocation);
  }

  ScopedStateTransitions(ScopedStateTransitions&&) = delete;
  ScopedStateTransitions(const ScopedStateTransitions&) = delete;
  ScopedStateTransitions& operator=(const ScopedStateTransitions&) = delete;
  ScopedStateTransitions& operator=(ScopedStateTransitions&&) = delete;

 private:
  TStateMachine* mImpl {nullptr};
  std::source_location mSourceLocation;
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
auto make_scoped_state_transitions(
  Detail::any_pointer auto&& stateMachine,
  const std::source_location& loc = std::source_location::current()) {
  auto smPtr
    = Detail::get_pointer(std::forward<decltype(stateMachine)>(stateMachine));
  static_assert(Detail::raw_pointer<decltype(smPtr)>);
  static_assert(!Detail::any_pointer<decltype(*smPtr)>);
  using TStateMachine = std::remove_cvref_t<decltype(*smPtr)>;
  return ScopedStateTransitions<TStateMachine, pre, state, post>(smPtr, loc);
}

template <class T>
consteval auto lockable_transitions() {
  using TT = Transition<T>;
  return std::array {
    TT {T::Unlocked, T::TryLock},
    TT {T::TryLock, T::Unlocked},
    TT {T::TryLock, T::Locked},
    TT {T::Locked, T::Unlocked},
  };
}
enum class LockStates {
  Unlocked,
  TryLock,
  Locked,
};
using LockState = StateMachine<
  LockStates,
  LockStates::Unlocked,
  lockable_transitions<LockStates>()>;

template <class T>
concept lockable_state_machine = requires() {
  { T::Values::Unlocked } -> std::same_as<typename T::Values>;
  { T::Values::TryLock } -> std::same_as<typename T::Values>;
  { T::Values::Locked } -> std::same_as<typename T::Values>;
}
// clang-format off
    && T::template IsValidTransition<T::Values::Unlocked, T::Values::TryLock>()
    && T::template IsValidTransition<T::Values::TryLock, T::Values::Locked>()
    && T::template IsValidTransition<T::Values::TryLock, T::Values::Unlocked>()
    && T::template IsValidTransition<T::Values::Locked, T::Values::Unlocked>()
    && !T::template IsValidTransition<T::Values::Locked, T::Values::TryLock>();
// clang-format on

static_assert(lockable_state_machine<LockState>);

}// namespace OpenKneeboard