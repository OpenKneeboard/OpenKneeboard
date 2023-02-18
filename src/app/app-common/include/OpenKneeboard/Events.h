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

#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <source_location>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "UniqueID.h"

namespace OpenKneeboard {

class EventContext final : public UniqueIDBase<EventContext> {};
class EventHandlerToken final : public UniqueIDBase<EventHandlerToken> {};
class EventHookToken final : public UniqueIDBase<EventHookToken> {};

template <class... Args>
using EventHandler = std::function<void(Args...)>;

class EventReceiver;

template <class... Args>
class Event;
class EventConnectionBase;
template <class... Args>
class EventConnection;

class EventBase {
  friend class EventConnectionBase;
  friend class EventReceiver;

 public:
  enum class HookResult {
    ALLOW_PROPAGATION,
    STOP_PROPAGATION,
  };

 protected:
  /** Event handlers are not invoked recursively to avoid deadlocks.
   *
   * If no calls are in progress in the current thread, this will immediately
   * invoke the specified handler, then invoke any other handlers that were
   * queued up while it was executing.
   *
   * If a call is in progress in the current thread, it will queue up the new
   * one, and return immediately.
   *
   * To similarly buffer events in a non-handler context, use the `EventDelay`
   * class.
   */
  static void InvokeOrEnqueue(std::function<void()>, std::source_location);

  virtual void RemoveHandler(EventHandlerToken) = 0;
  std::recursive_mutex mMutex;
};

/** Delay any event handling in the current thread for the lifetime of this
 * class.
 *
 * For example, you may want to use this after */
class EventDelay final {
 public:
  EventDelay();
  ~EventDelay();

  EventDelay(const EventDelay&) = delete;
  EventDelay(EventDelay&&) = delete;
  auto operator=(const EventDelay&) = delete;
  auto operator=(EventDelay&&) = delete;

 private:
  bool mOwner = false;
};

class EventConnectionBase {
 public:
  EventHandlerToken mToken;
  virtual void Invalidate() = 0;
};

template <class... Args>
class EventConnection final
  : public EventConnectionBase,
    public std::enable_shared_from_this<EventConnection<Args...>> {
 private:
  EventConnection(EventHandler<Args...> handler, std::source_location location)
    : mHandler(handler), mSourceLocation(location) {
  }

 public:
  EventConnection() = delete;

  static std::shared_ptr<EventConnection<Args...>> Create(
    EventHandler<Args...> handler,
    std::source_location location) {
    return std::shared_ptr<EventConnection<Args...>>(
      new EventConnection(handler, location));
  }

  constexpr operator bool() const noexcept {
    // not bothering with the lock, as it's checked with lock in Call() and
    // Invalidate() anyway
    return static_cast<bool>(mHandler);
  }

  void Call(Args... args) {
    auto stayingAlive = this->shared_from_this();

    decltype(mHandler) handler;
    {
      std::unique_lock lock(mMutex);
      handler = mHandler;
    }
    if (handler) {
      // In release builds, ignore but drop unhandled exceptions from handlers.
      // In debug builds, break (or crash)
      try {
        handler(args...);
      } catch (const std::exception& e) {
        dprintf("Uncaught std::exception from event handler: {}", e.what());
        OPENKNEEBOARD_BREAK;
      } catch (const winrt::hresult_error& e) {
        dprintf(
          L"Uncaught hresult error from event handler: {} - {}",
          e.code().value,
          std::wstring_view {e.message()});
        OPENKNEEBOARD_BREAK;
      } catch (...) {
        dprint("Uncaught unknown exception from event handler");
        OPENKNEEBOARD_BREAK;
      }
    }
  }

  virtual void Invalidate() override {
    auto stayingAlive = this->shared_from_this();
    std::unique_lock lock(mMutex);
    mHandler = {};
  }

 private:
  EventHandler<Args...> mHandler;
  std::source_location mSourceLocation;
  std::recursive_mutex mMutex;
};

/** a 1:n event. */
template <class... Args>
class Event final : public EventBase {
  friend class EventReceiver;

 public:
  using Hook = std::function<HookResult(Args...)>;

  Event() = default;
  Event(const Event<Args...>&) = delete;
  Event& operator=(const Event<Args...>&) = delete;
  ~Event();

  void Emit(
    Args... args,
    std::source_location location = std::source_location::current());

  EventHookToken AddHook(Hook, EventHookToken token = {}) noexcept;
  void RemoveHook(EventHookToken) noexcept;

 protected:
  std::shared_ptr<EventConnectionBase> AddHandler(
    const EventHandler<Args...>&,
    std::source_location current);
  virtual void RemoveHandler(EventHandlerToken token) override;

 private:
  std::
    unordered_map<EventHandlerToken, std::shared_ptr<EventConnection<Args...>>>
      mReceivers;
  std::unordered_map<EventHookToken, Hook> mHooks;
};

class EventReceiver {
  friend class EventBase;
  template <class... Args>
  friend class Event;

 public:
  EventReceiver();
  /* You **MUST** call RemoveAllEventListeners() in your
   * subclass destructor - otherwise other threads may
   * invoke an event handler while your object is partially
   * destructed */
  virtual ~EventReceiver();

  EventReceiver(const EventReceiver&) = delete;
  EventReceiver& operator=(const EventReceiver&) = delete;

 protected:
  std::vector<std::shared_ptr<EventConnectionBase>> mSenders;

  // `std::type_identity_t` makes the compiler infer `Args` from the event,
  // then match the handler, instead of attempting to infer `Args from both.
  template <class... Args>
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    const std::type_identity_t<EventHandler<Args...>>& handler,
    std::source_location location = std::source_location::current());

  template <class... Args>
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    std::type_identity_t<Event<Args...>>& forwardAs,
    std::source_location location = std::source_location::current());

  template <class... Args>
    requires(sizeof...(Args) > 0)
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    Event<>& forwardAs,
    std::source_location location = std::source_location::current());

  template <class... Args>
    requires(sizeof...(Args) > 0)
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    const std::function<void()>& handler,
    std::source_location location = std::source_location::current());

  void RemoveEventListener(EventHandlerToken);
  void RemoveAllEventListeners();

 private:
  std::recursive_mutex mMutex;
};

template <class... Args>
std::shared_ptr<EventConnectionBase> Event<Args...>::AddHandler(
  const EventHandler<Args...>& handler,
  std::source_location location) {
  std::unique_lock lock(mMutex);
  auto connection = EventConnection<Args...>::Create(handler, location);
  auto token = connection->mToken;
  mReceivers.emplace(token, connection);
  return std::move(connection);
}

template <class... Args>
void Event<Args...>::RemoveHandler(EventHandlerToken token) {
  std::shared_ptr<EventConnectionBase> receiver;
  {
    std::unique_lock lock(mMutex);
    if (!mReceivers.contains(token)) {
      return;
    }
    receiver = mReceivers.at(token);
    mReceivers.erase(token);
  }
  receiver->Invalidate();
}

template <class... Args>
void Event<Args...>::Emit(Args... args, std::source_location location) {
  //  Copy in case one is erased while we're running
  decltype(mHooks) hooks;
  std::vector<std::shared_ptr<EventConnection<Args...>>> receivers;
  {
    std::unique_lock lock(mMutex);
    hooks = mHooks;
    auto it = mReceivers.begin();
    while (it != mReceivers.end()) {
      auto receiver = it->second;
      if (!receiver) {
        it = mReceivers.erase(it);
        continue;
      }
      receivers.push_back(receiver);
      ++it;
    }
  }

  for (const auto& [_, hook]: hooks) {
    if (hook(args...) == HookResult::STOP_PROPAGATION) {
      return;
    }
  }

  InvokeOrEnqueue(
    [=]() {
      for (const auto& receiver: receivers) {
        receiver->Call(args...);
      }
    },
    location);
}

template <class... Args>
Event<Args...>::~Event() {
  decltype(mReceivers) receivers;
  {
    std::unique_lock lock(mMutex);
    auto receivers = mReceivers;
  }
  for (const auto& [token, receiver]: receivers) {
    receiver->Invalidate();
  }
}

template <class... Args>
EventHookToken Event<Args...>::AddHook(
  Hook hook,
  EventHookToken token) noexcept {
  std::unique_lock lock(mMutex);
  mHooks.insert_or_assign(token, hook);
  return token;
}

template <class... Args>
void Event<Args...>::RemoveHook(EventHookToken token) noexcept {
  std::unique_lock lock(mMutex);
  auto it = mHooks.find(token);
  if (it != mHooks.end()) {
    mHooks.erase(it);
  }
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::type_identity_t<EventHandler<Args...>>& handler,
  std::source_location location) {
  std::unique_lock lock(mMutex);
  mSenders.push_back(event.AddHandler(handler, location));
  return mSenders.back()->mToken;
}

template <class... Args>
  requires(sizeof...(Args) > 0)
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::function<void()>& handler,
  std::source_location location) {
  return AddEventListener(
    event, [handler](Args...) { handler(); }, location);
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  std::type_identity_t<Event<Args...>>& forwardTo,
  std::source_location location) {
  return AddEventListener(
    event,
    [location, &forwardTo](Args... args) { forwardTo.Emit(args..., location); },
    location);
}

template <class... Args>
  requires(sizeof...(Args) > 0)
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  Event<>& forwardTo,
  std::source_location location) {
  return AddEventListener(
    event,
    [location, &forwardTo](Args...) { forwardTo.Emit(location); },
    location);
}

}// namespace OpenKneeboard
