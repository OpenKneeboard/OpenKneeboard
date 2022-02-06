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
#include <map>
#include <type_traits>
#include <vector>

namespace OpenKneeboard {

template <class... Args>
using EventHandler = std::function<void(Args...)>;

class EventReceiver;

template <class... Args>
class Event;

class EventBase {
  friend class EventReceiver;

 protected:
  void Add(EventReceiver*, uint64_t);
  virtual void RemoveHandler(uint64_t token) = 0;
};

/** a 1:n event.
 *
 * wxWidgets are 1:1 events
 */
template <class... Args>
class Event final : public EventBase {
  friend class EventReceiver;
 public:
  Event() = default;
  Event(const Event<Args...>&) = delete;
  Event& operator=(const Event<Args...>&) = delete;
  ~Event();

  void operator()(Args&&... args);

 protected:
  void AddHandler(EventReceiver*, const EventHandler<Args...>&);
  virtual void RemoveHandler(uint64_t token) override;

 private:
  struct ReceiverInfo {
    EventReceiver* mReceiver;
    EventHandler<Args...> mFunc;
  };
  uint64_t mNextToken = 0;
  std::unordered_map<uint64_t, ReceiverInfo> mReceivers;
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
    uint64_t mToken;
  };
  std::vector<SenderInfo> mSenders;

  // `std::type_identity_t` makes the compiler infer `Args` from the event, then
  // match the handler, instead of attempting to infer `Args from both.
  template <class... Args>
  void AddEventListener(
    Event<Args...>& event,
    const std::type_identity_t<EventHandler<Args...>>& handler);
  template <class... Args>
  void AddEventListener(
    Event<Args...>& event,
    std::type_identity_t<Event<Args...>>& forwardAs);
  template <class... Args>
  void AddEventListener(
    Event<Args...>& event,
    const std::function<void()>& handler);

  template <class... EventArgs, class Func, class First, class... Rest>
  void AddEventListener(
    Event<EventArgs...>& event,
    Func f,
    First&& first,
    Rest&&... rest) {
    AddEventListener(
      event,
      std::bind_front(
        f, std::forward<First>(first), std::forward<Rest>(rest)...));
  }
};

template <class... Args>
void Event<Args...>::AddHandler(
  EventReceiver* receiver,
  const EventHandler<Args...>& handler) {
  const auto token = mNextToken++;
  mReceivers.emplace(token, ReceiverInfo {receiver, handler});
  this->Add(receiver, token);
}

template <class... Args>
void Event<Args...>::RemoveHandler(uint64_t token) {
  mReceivers.erase(token);
}

template <class... Args>
void Event<Args...>::operator()(Args&&... args) {
  for (const auto& [token, info]: mReceivers) {
    info.mFunc(std::forward<Args>(args)...);
  }
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
void EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::type_identity_t<EventHandler<Args...>>& handler) {
  event.AddHandler(this, handler);
}

template <class... Args>
void EventReceiver::AddEventListener(
  Event<Args...>& event,
  const std::function<void()>& handler) {
  event.AddHandler(this, [handler](Args...) { handler(); });
}

template <class... Args>
void EventReceiver::AddEventListener(
  Event<Args...>& event,
  std::type_identity_t<Event<Args...>>& forwardTo) {
  event.AddHandler(this, [&](Args... args) { forwardTo(args...); });
}

}// namespace OpenKneeboard
