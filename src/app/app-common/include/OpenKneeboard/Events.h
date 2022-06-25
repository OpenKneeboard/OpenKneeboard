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

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <type_traits>
#include <vector>

#include "UniqueID.h"

namespace OpenKneeboard {

class EventContext final : public UniqueIDBase<EventContext> {};
class EventHandlerToken final : public UniqueIDBase<EventHandlerToken> {};

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
  virtual void RemoveHandler(EventHandlerToken) = 0;
  std::recursive_mutex mMutex;
};

class EventConnectionBase {
 public:
  EventHandlerToken mToken;
  EventBase* mSender;
  void Invalidate();

 protected:
  virtual void InvalidateImpl() = 0;
  std::recursive_mutex mMutex;
};

template <class... Args>
class EventConnection : public EventConnectionBase {
 public:
  EventConnection(EventBase* sender, EventHandler<Args...> handler)
    : mHandler(handler) {
    mSender = sender;
  }

  void Call(Args... args) {
    std::unique_lock lock(mMutex);
    if (mHandler) {
      mHandler(args...);
    }
  }

 protected:
  virtual void InvalidateImpl() override {
    mHandler = {};
  }

 private:
  EventHandler<Args...> mHandler;
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

  void Emit(Args... args);

  void PushHook(Hook);
  void PopHook();

 protected:
  std::shared_ptr<EventConnectionBase> AddHandler(const EventHandler<Args...>&);
  virtual void RemoveHandler(EventHandlerToken token) override;

 private:
  std::
    unordered_map<EventHandlerToken, std::shared_ptr<EventConnection<Args...>>>
      mReceivers;
  std::list<Hook> mHooks;
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
  bool mCleared = false;
  std::vector<std::shared_ptr<EventConnectionBase>> mSenders;

  // `std::type_identity_t` makes the compiler infer `Args` from the event, then
  // match the handler, instead of attempting to infer `Args from both.
  template <class... Args>
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    const std::type_identity_t<EventHandler<Args...>>& handler);

  template <class... Args>
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    std::type_identity_t<Event<Args...>>& forwardAs);

  template <class... Args>
    requires(sizeof...(Args) > 0)
  EventHandlerToken AddEventListener(Event<Args...>& event, Event<>& forwardAs);

  template <class... Args>
    requires(sizeof...(Args) > 0)
  EventHandlerToken AddEventListener(
    Event<Args...>& event,
    const std::function<void()>& handler);

  template <class... EventArgs, class Func, class First, class... Rest>
  EventHandlerToken AddEventListener(
    Event<EventArgs...>& event,
    Func f,
    First&& first,
    Rest&&... rest) {
    return AddEventListener(
      event,
      std::bind_front(
        f, std::forward<First>(first), std::forward<Rest>(rest)...));
  }

  void RemoveEventListener(EventHandlerToken);
  void RemoveAllEventListeners();

 private:
  std::recursive_mutex mMutex;
};

template <class... Args>
std::shared_ptr<EventConnectionBase> Event<Args...>::AddHandler(
  const EventHandler<Args...>& handler) {
  std::unique_lock lock(mMutex);
  auto connection = std::make_shared<EventConnection<Args...>>(this, handler);
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
void Event<Args...>::Emit(Args... args) {
  //  Copy in case one is erased while we're running
  decltype(mHooks) hooks;
  decltype(mReceivers) receivers;
  {
    std::unique_lock lock(mMutex);
    hooks = mHooks;
    receivers = mReceivers;
  }

  for (const auto& hook: hooks) {
    if (hook(args...) == HookResult::STOP_PROPAGATION) {
      return;
    }
  }
  for (const auto& [token, receiver]: receivers) {
    receiver->Call(args...);
  }
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
void Event<Args...>::PushHook(Hook hook) {
  mHooks.push_front(hook);
}

template <class... Args>
void Event<Args...>::PopHook() {
  mHooks.pop_front();
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::type_identity_t<EventHandler<Args...>>& handler) {
  std::unique_lock lock(mMutex);
  mCleared = false;
  mSenders.push_back(event.AddHandler(handler));
  return mSenders.back()->mToken;
}

template <class... Args>
  requires(sizeof...(Args) > 0)
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::function<void()>& handler) {
  return AddEventListener(event, [handler](Args...) { handler(); });
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  std::type_identity_t<Event<Args...>>& forwardTo) {
  return AddEventListener(
    event, [&forwardTo](Args... args) { forwardTo.Emit(args...); });
}

template <class... Args>
  requires(sizeof...(Args) > 0)
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  Event<>& forwardTo) {
  return AddEventListener(event, [&forwardTo](Args...) { forwardTo.Emit(); });
}

}// namespace OpenKneeboard
