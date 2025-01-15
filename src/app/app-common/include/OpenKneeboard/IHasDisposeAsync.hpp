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

#include <OpenKneeboard/StateMachine.hpp>

#include <OpenKneeboard/task.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Foundation.h>

namespace OpenKneeboard {
class IHasDisposeAsync {
 public:
  // This is an alternative to `final_release()`, where consumers (or
  // subclasses) may need to be able to wait for async cleanup
  [[nodiscard]]
  virtual task<void> DisposeAsync() noexcept
    = 0;
};

/** Helper for tracking disposal states.
 *
 * Usage:
 *
 * ```
 * DisposalState mDisposal;
 *
 * ...
 * task<void> DisposeAsync() noexcept {
 *   const auto disposing = mDisposal.Start();
 *   if (!disposing) {
 *     co_return;
 *   }
 *   ...
 * }
 *
 * ...
 *
 * void DoThing() noexcept {
 *   if (mDisposal.HaveStarted()) {
 *     throw new Exception("Can't DoThing() after disposal()");
 *   }
 *   // ...
 * }
 * ```
 */
class DisposalState final {
 private:
  enum class State {
    Live,
    Disposing,
    Disposed,
  };
  using StateMachine = AtomicStateMachine<
    State,
    State::Live,
    std::array {
      Transition {State::Live, State::Disposing},
      Transition {State::Disposing, State::Disposed},
    },
    State::Disposed>;
  friend class UniqueDisposal;

  class UniqueDisposal {
   public:
    UniqueDisposal() = default;
    UniqueDisposal(const UniqueDisposal&) = delete;
    UniqueDisposal& operator=(const UniqueDisposal&) = delete;

    UniqueDisposal& operator=(UniqueDisposal&& other) {
      mStateMachine = other.mStateMachine;
      other.mStateMachine = nullptr;
      return *this;
    }

    UniqueDisposal(UniqueDisposal&& other) {
      *this = std::move(other);
    }

    constexpr UniqueDisposal(StateMachine* impl) : mStateMachine(impl) {
    }

    inline ~UniqueDisposal() {
      if (mStateMachine) {
        mStateMachine->Transition<State::Disposing, State::Disposed>();
      }
    }

    constexpr operator bool() const noexcept {
      return mStateMachine;
    }

   private:
    StateMachine* mStateMachine {nullptr};
  };

 public:
  constexpr DisposalState(
    const std::source_location& caller = std::source_location::current())
    : mStateMachine(caller) {
  }

  /** Start, wait or pending, or return immediately.
   *
   * Named for consistency with `std::call_once()`
   */
  [[nodiscard]]
  inline task<UniqueDisposal> StartOnce() noexcept {
    auto ret = this->Start();
    if (!ret) {
      co_await winrt::resume_background();
      mStateMachine.Wait(State::Disposing);
    }
    co_return std::move(ret);
  }

  constexpr bool HasStarted() const noexcept {
    return mStateMachine.Get() != State::Live;
  }

 private:
  StateMachine mStateMachine;

  [[nodiscard]]
  inline UniqueDisposal Start() noexcept {
    if (mStateMachine.TryTransition<State::Live, State::Disposing>()) {
      return {&mStateMachine};
    } else {
      return {nullptr};
    }
  }
};

}// namespace OpenKneeboard