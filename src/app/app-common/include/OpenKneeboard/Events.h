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

class EventBase {
  friend class EventReceiver;

 public:
  enum class HookResult {
    ALLOW_PROPAGATION,
    STOP_PROPAGATION,
  };

 protected:
  static void EnqueueForMainThread(std::function<void()>);
  EventHandlerToken AddHandler(EventReceiver*);
  virtual void RemoveHandler(EventHandlerToken) = 0;
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
  void EmitFromMainThread(Args... args);

  void PushHook(Hook);
  void PopHook();

 protected:
  EventHandlerToken AddHandler(EventReceiver*, const EventHandler<Args...>&);
  virtual void RemoveHandler(EventHandlerToken token) override;

 private:
  struct ReceiverInfo {
    EventReceiver* mReceiver;
    EventHandler<Args...> mFunc;
  };
  std::unordered_map<EventHandlerToken, ReceiverInfo> mReceivers;
  std::list<Hook> mHooks;
};

class EventReceiver {
  friend class EventBase;
  template <class... Args>
  friend class Event;

 public:
  EventReceiver();
  virtual ~EventReceiver();

  EventReceiver(const EventReceiver&) = delete;
  EventReceiver& operator=(const EventReceiver&) = delete;

 protected:
  struct SenderInfo {
    EventBase* mEvent;
    EventHandlerToken mToken;
  };
  std::vector<SenderInfo> mSenders;

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
};

template <class... Args>
EventHandlerToken Event<Args...>::AddHandler(
  EventReceiver* receiver,
  const EventHandler<Args...>& handler) {
  const auto token = EventBase::AddHandler(receiver);
  mReceivers.emplace(token, ReceiverInfo {receiver, handler});
  return token;
}

template <class... Args>
void Event<Args...>::RemoveHandler(EventHandlerToken token) {
  mReceivers.erase(token);
}

template <class... Args>
void Event<Args...>::Emit(Args... args) {
  // Copy in case one is erased while we're running
  auto hooks = mHooks;
  for (const auto& hook: hooks) {
    if (hook(args...) == HookResult::STOP_PROPAGATION) {
      return;
    }
  }
  for (const auto& [token, info]: mReceivers) {
    info.mFunc(args...);
  }
}

template <class... Args>
void Event<Args...>::EmitFromMainThread(Args... args) {
  EventBase::EnqueueForMainThread([=]() { this->Emit(args...); });
}

template <class... Args>
Event<Args...>::~Event() {
  for (const auto& receiverIt: mReceivers) {
    auto info = receiverIt.second;
    auto& receiverHandlers = info.mReceiver->mSenders;
    for (auto it = receiverHandlers.begin(); it != receiverHandlers.end();) {
      if (it->mEvent == this) {
        it = receiverHandlers.erase(it);
      } else {
        ++it;
      }
    }
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
  return event.AddHandler(this, handler);
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::function<void()>& handler) {
  return event.AddHandler(this, [handler](Args...) { handler(); });
}

template <class... Args>
EventHandlerToken EventReceiver::AddEventListener(
  Event<Args...>& event,
  std::type_identity_t<Event<Args...>>& forwardTo) {
  return event.AddHandler(this, [&](Args... args) { forwardTo.Emit(args...); });
}

}// namespace OpenKneeboard
