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
 public:
  Event() = default;
  Event(const Event<Args...>&) = delete;
  Event& operator=(const Event<Args...>&) = delete;
  ~Event();

  void AddHandler(EventReceiver*, const EventHandler<Args...>&);
  void operator()(Args... args);

 protected:
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
void Event<Args...>::operator()(Args... args) {
  for (const auto& [token, info]: mReceivers) {
    info.mFunc(args...);
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

}// namespace OpenKneeboard
