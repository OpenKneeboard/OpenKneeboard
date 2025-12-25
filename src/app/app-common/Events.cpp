// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/Events.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>

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
      fatal("Shutting down the event system twice");
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
        fatal("Event count = 0, but not shutting down");
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
  ++queue.mDelayDepth;
}

EventDelay::~EventDelay() {
  auto& queue = ThreadData::Get();
  const auto count = --queue.mDelayDepth;
  if (!count) {
    queue.Flush();
  }
}

}// namespace OpenKneeboard
