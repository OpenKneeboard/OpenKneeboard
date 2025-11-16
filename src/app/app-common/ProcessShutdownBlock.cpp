// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/ProcessShutdownBlock.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <atomic>
#include <source_location>
#include <unordered_map>

namespace OpenKneeboard {

namespace {

struct ShutdownData {
 public:
  static ShutdownData& Get() {
    static ShutdownData sInstance;
    return sInstance;
  }

  uint64_t Increment(const std::source_location& loc) noexcept {
    mBlockCount.fetch_add(1);
    if (mShuttingDown.test()) {
      fatal(loc, "Incrementing after shutdown");
    }
    const auto id = mNextID++;
    std::unique_lock lock(mMutex);
    mActiveBlocks.emplace(id, loc);
    return id;
  }

  void Decrement(uint64_t id) noexcept {
    {
      std::unique_lock lock(mMutex);
      if (mShuttingDown.test()) {
        const auto loc = mActiveBlocks.at(id);
        dprint("Shutdown cleanup @ {}", loc);
      }
      mActiveBlocks.erase(id);
    }

    const auto remaining = mBlockCount.fetch_sub(1) - 1;

    if (mShuttingDown.test()) {
      dprint("{} shutdown items remaining.", remaining);
    }

    if (remaining == 0) {
      if (!mShuttingDown.test()) [[unlikely]] {
        fatal("Block count = 0, but not shutting down");
      }
      SetEvent(mShutdownEvent);
    }
  }

  void SetEventOnCompletion(HANDLE completionEvent) noexcept {
    mShutdownEvent = completionEvent;

    {
      std::unique_lock lock(mMutex);
      if (mShuttingDown.test_and_set()) {
        fatal("Running shutdown blockers twice");
      }

      this->DumpActiveBlocks();
    }

    this->Decrement(mMyID);
  }

  void DumpActiveBlocks() noexcept {
    std::unique_lock lock(mMutex, std::try_to_lock);
    dprint("Waiting for {} shutdown blockers:", mActiveBlocks.size());
    for (const auto& [id, location]: mActiveBlocks) {
      dprint("- {}", location);
    }
  }

 private:
  ShutdownData() {
    mMyID = this->Increment(std::source_location::current());
  }

  std::atomic_uint64_t mBlockCount {0};
  std::atomic_flag mShuttingDown;
  std::atomic_uint64_t mNextID {0};

  uint64_t mMyID;

  HANDLE mShutdownEvent {};

  // Keeping the `std::atomic_foo` instead of using the mutex for everything in
  // case I decide to just do the full tracking in debug builds
  std::mutex mMutex;
  std::unordered_map<uint64_t, std::source_location> mActiveBlocks;
};
}// namespace

ProcessShutdownBlock::ProcessShutdownBlock(const std::source_location& loc)
  : mID(ShutdownData::Get().Increment(loc)) {
}

ProcessShutdownBlock::~ProcessShutdownBlock() {
  ShutdownData::Get().Decrement(mID);
}

void ProcessShutdownBlock::SetEventOnCompletion(
  HANDLE completionEvent) noexcept {
  ShutdownData::Get().SetEventOnCompletion(completionEvent);
}

void ProcessShutdownBlock::DumpActiveBlocks() noexcept {
  ShutdownData::Get().DumpActiveBlocks();
}

}// namespace OpenKneeboard