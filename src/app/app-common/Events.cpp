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
#include <OpenKneeboard/Events.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/scope_exit.hpp>

#include <queue>

namespace OpenKneeboard {

static uint64_t sNextUniqueID = 0x1234abcdui64 << 32;

uint64_t _UniqueIDImpl::GetAndIncrementNextValue() {
  return sNextUniqueID++;
}

EventReceiver::EventReceiver() {
}

EventReceiver::~EventReceiver() {
  if (!mSenders.empty()) {
    dprint(
      "I'm in danger! ~EventReceiver() called without "
      "RemoveAllEventListeners()");
    OPENKNEEBOARD_BREAK;
  }
  this->RemoveAllEventListeners();
}

void EventReceiver::RemoveAllEventListeners() {
  decltype(mSenders) senders;
  {
    senders = mSenders;
    mSenders.clear();
  }

  for (auto& sender: senders) {
    sender->Invalidate();
  }
}

void EventReceiver::RemoveEventListener(EventHandlerToken token) {
  std::shared_ptr<EventConnectionBase> toInvalidate;
  {
    for (auto it = mSenders.begin(); it != mSenders.end(); ++it) {
      auto& sender = *it;
      if (sender->mToken != token) {
        continue;
      }
      toInvalidate = sender;
      mSenders.erase(it);
      break;
    }
  }
  if (toInvalidate) {
    toInvalidate->Invalidate();
  }
}

namespace {
struct EmitterQueueItem {
  std::function<void()> mEmitter;
  std::source_location mEnqueuedFrom;
};

struct GlobalData {
  bool StartEvent() {
    mEventCount.fetch_add(1);
    if (mShuttingDown.test()) {
      this->Sub1();
      return false;
    }
    return true;
  }

  void FinishEvent() {
    this->Sub1();
  }

  void Shutdown(HANDLE event) {
    mShutdownEvent = event;
    if (mShuttingDown.test_and_set()) {
      OPENKNEEBOARD_LOG_AND_FATAL("Shutting down the event system twice");
    }

    this->Sub1();
  }

  static auto& Get() {
    static GlobalData sInstance;
    return sInstance;
  }

 private:
  GlobalData() = default;

  void Sub1() {
    if (mEventCount.fetch_sub(1) == 1) {
      if (!mShuttingDown.test()) [[unlikely]] {
        OPENKNEEBOARD_LOG_AND_FATAL("Event count = 0, but not shutting down");
      }
      SetEvent(mShutdownEvent);
    }
  }

  std::atomic_uint64_t mEventCount {1};
  std::atomic_flag mShuttingDown;
  HANDLE mShutdownEvent {};
};

struct ThreadData {
  uint64_t mDelayDepth = 0;

  static auto& Get() {
    static thread_local ThreadData sInstance;
    return sInstance;
  }

  void EmitOrEnqueue(const EmitterQueueItem& item) noexcept {
    auto& globals = GlobalData::Get();
    if (!globals.StartEvent()) {
      return;
    }
    if (mDelayDepth > 0) {
      mEmitterQueue.push(item);
      return;
    }

    item.mEmitter();
    globals.FinishEvent();
  }

  void Flush() noexcept {
    auto& globals = GlobalData::Get();
    while (!mEmitterQueue.empty()) {
      auto item = mEmitterQueue.front();
      mEmitterQueue.pop();
      item.mEmitter();
      globals.FinishEvent();
    }
  }

 private:
  ThreadData() = default;

  std::queue<EmitterQueueItem> mEmitterQueue;
};

}// namespace

void EventBase::Shutdown(HANDLE event) {
  GlobalData::Get().Shutdown(event);
}

void EventBase::InvokeOrEnqueue(
  std::function<void()> func,
  std::source_location location) {
  auto& queue = ThreadData::Get();
  queue.EmitOrEnqueue({func, location});
}

EventDelay::EventDelay(std::source_location source) : mSourceLocation(source) {
  auto& queue = ThreadData::Get();
  const auto count = ++queue.mDelayDepth;
  TraceLoggingWriteStart(
    mActivity,
    "EventDelay",
    TraceLoggingValue(count, "Depth"),
    OPENKNEEBOARD_TraceLoggingSourceLocation(mSourceLocation));
}

EventDelay::~EventDelay() {
  auto& queue = ThreadData::Get();
  const auto count = --queue.mDelayDepth;
  if (!count) {
    queue.Flush();
  }
  TraceLoggingWriteStop(
    mActivity,
    "EventDelay",
    TraceLoggingValue(queue.mDelayDepth, "Depth"),
    OPENKNEEBOARD_TraceLoggingSourceLocation(mSourceLocation));
  return;
}

}// namespace OpenKneeboard
