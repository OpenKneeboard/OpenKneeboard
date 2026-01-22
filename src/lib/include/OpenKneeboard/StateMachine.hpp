// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/enum.hpp>

#include <array>
#include <atomic>
#include <concepts>
#include <expected>
#include <format>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace OpenKneeboard {

/** This file provides a state machine implementation with
 * statically-verified transitions.
 *
 * The main classes are:
 * - `Transition`: a source -> destination pair
 * - `StateMachine`: probably what you want
 * - `AtomicStateMachine`: a state machine using `std::atomic<>` for storage
 * - `StateMachineBase`: shared implementation detail of `StateMachine` and
 *   `AtomicStateMachine`; you probably don't want to use this directly.
 *
 * Usage:
 *
 * ```
 * enum class MyStates { Foo, Bar, ... };
 * using MyStateMachine = StateMachine<
 *   MyStates,
 *   MyStates::Foo, // initial state
 *   std::array {
 *     Transition { MyStates::Foo, MyStates::Bar },
 *   },
 *   MyStates::Bar // final state
 * >;
 *
 * MyStateMachine sm;
 *
 * // - Compile-time failure if Foo -> Bar is not in the list of valid
 * transitions
 * // - Runtime-failure if current state is not `Foo`:
 * sm.Transition<MyStates::Foo, MyStates::Bar>();
 *
 * // alternatively:
 * // - compile-time failure if Foo -> Bar is not in the list of valid
 * //   transitions
 * // - get an `std::expected<void, State>` - error is the current state if not
 * //   `Foo`:
 *
 * const auto result = sm.TryTransition<MyStates::Foo, MyStates::Bar>()`
 *
 * auto state = sm.Get();
 * sm.Assert(MyStates::Bar);
 * ```
 *
 * - you can replace `StateMachine` with `AtomicStateMachine` if you require
 *   `std::atomic`'s usual behavior
 * - you can omit the final state parameter, or provide `std::nullopt`
 */

template <class State>
  requires std::is_scoped_enum_v<State>
struct Transition final {
  Transition() = delete;
  consteval Transition(State in, State out) : mIn(in), mOut(out) {}

  const State mIn;
  const State mOut;
};

template <class T, size_t N>
using Transitions = std::array<Transition<T>, N>;

template <class T, auto V>
constexpr bool is_optional_value_v =
  std::same_as<std::nullopt_t, decltype(V)> || std::same_as<T, decltype(V)>;

/// Use either StateMachine or AtomicStateMachine instead
template <
  class State,
  class StateContainer,
  State TInitialState,
  size_t TransitionCount,
  std::array<Transition<State>, TransitionCount> Transitions,
  auto TFinalState>
  requires(TransitionCount >= 1)
  && std::is_scoped_enum_v<State> && is_optional_value_v<State, TFinalState>
class StateMachineBase {
 public:
  using Values = State;
  static constexpr auto InitialState = TInitialState;
  static constexpr auto FinalState = TFinalState;
  static constexpr auto HasFinalState =
    std::same_as<State, std::decay_t<decltype(FinalState)>>;

  StateMachineBase() = delete;
  constexpr StateMachineBase(
    const std::source_location& creator = std::source_location::current())
    : mCreator(creator) {}

  ~StateMachineBase() {
    if constexpr (HasFinalState) {
      this->Assert(FinalState, "Unexpected final state", mCreator);
    }
  }

  constexpr State Get() const noexcept { return mState; }

  constexpr void Assert(
    State expected,
    std::string_view message = "Assertion failure",
    const std::source_location& caller = std::source_location::current()) {
    const auto actual = Get();
    if (actual == expected) [[likely]] {
      return;
    }
    fatal(
      caller,
      "{}: Expected state {:#}, but state is {:#}",
      message,
      expected,
      actual);
  }

  StateMachineBase(const StateMachineBase&) = delete;
  StateMachineBase(StateMachineBase&&) = delete;
  StateMachineBase& operator=(const StateMachineBase&) = delete;
  StateMachineBase& operator=(StateMachineBase&&) = delete;

  template <State in, State out>
  constexpr void Transition(
    this auto&& self,
    const std::source_location& loc = std::source_location::current()) {
    const auto result = self.template TryTransition<in, out>();
    if (result) [[likely]] {
      return;
    }

    fatal(
      loc, "Unexpected state {:#}; expected {} -> {}", result.error(), in, out);
  }

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

 private:
  std::source_location mCreator;
};

template <
  class State,
  State InitialState,
  auto Transitions,
  auto FinalState = std::nullopt,
  class Base = StateMachineBase<
    State,
    State,
    InitialState,
    Transitions.size(),
    Transitions,
    FinalState>>
class StateMachine final : public Base {
 public:
  constexpr StateMachine(
    const std::source_location& caller = std::source_location::current())
    : Base(caller) {}

  template <State in, State out>
  [[nodiscard]]
  constexpr std::expected<void, State> TryTransition()
    requires(Base::template IsValidTransition<in, out>())
  {
    if (this->mState != in) [[unlikely]] {
      return std::unexpected {this->mState};
    }
    this->mState = out;
    return {};
  }
};

template <
  class State,
  State InitialState,
  auto Transitions,
  auto FinalState = std::nullopt,
  class Base = StateMachineBase<
    State,
    std::atomic<State>,
    InitialState,
    Transitions.size(),
    Transitions,
    FinalState>>
class AtomicStateMachine final : public Base {
 public:
  constexpr AtomicStateMachine(
    const std::source_location& caller = std::source_location::current())
    : Base(caller) {}

  /** Attempt to transition, and fail with the current state if it is not the
   * expected state */
  template <State in, State out>
  [[nodiscard]]
  constexpr std::expected<void, State> TryTransition()
    requires(Base::template IsValidTransition<in, out>())
  {
    auto current = in;
    if (!this->mState.compare_exchange_strong(current, out)) {
      return std::unexpected {current};
    }
    return {};
  }

  constexpr State Get(
    std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return this->mState.load(order);
  }

  void Wait(State old, std::memory_order order = std::memory_order_seq_cst)
    const noexcept {
    this->mState.wait(old, order);
  }
};

template <class TStateMachine, auto pre, auto state, auto post>
class ScopedStateTransitions final {
 public:
  ScopedStateTransitions() = delete;
  constexpr ScopedStateTransitions(
    TStateMachine* impl,
    const std::source_location& loc = std::source_location::current())
    : mImpl(impl),
      mSourceLocation(loc) {
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
  auto smPtr =
    Detail::get_pointer(std::forward<decltype(stateMachine)>(stateMachine));
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
concept lockable_state_machine =
  requires() {
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
