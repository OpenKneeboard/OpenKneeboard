// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/dprint.hpp>

#include <atomic>
#include <source_location>
#include <unordered_map>

namespace OpenKneeboard {

/// Wraps std::shared_ptr to add accounting, to track down leaks
template <class T>
class audited_ptr {
 private:
  uint64_t mRefID {~(0ui64)};

  struct RefData {
    std::unordered_map<uint64_t, std::source_location> mRefs;
    std::atomic_uint64_t mNextID {0};
  };

  std::shared_ptr<T> mImpl;
  std::shared_ptr<RefData> mRefData;

 public:
  audited_ptr() = default;
  audited_ptr(nullptr_t) {
  }

  audited_ptr(
    T* ptr,
    const std::source_location& loc = std::source_location::current()) {
    mImpl = std::shared_ptr<T>(ptr);
    mRefData = std::make_shared<RefData>();

    mRefID = mRefData->mNextID++;
    mRefData->mRefs.emplace(mRefID, loc);
  }

  audited_ptr(
    const audited_ptr& other,
    const std::source_location& loc = std::source_location::current()) {
    copy_from(other, loc);
  }

  audited_ptr(
    audited_ptr&& other,
    const std::source_location& loc
    = std::source_location::current()) noexcept {
    copy_from(other, loc);
  }

  ~audited_ptr() noexcept {
    if (!mRefData) {
      return;
    }
    mRefData->mRefs.erase(mRefID);
  }

  T* get() const noexcept {
    return mImpl.get();
  }

  T& operator*() const noexcept {
    return *mImpl;
  }

  T* operator->() const noexcept {
    return mImpl.get();
  }

  audited_ptr& operator=(nullptr_t) {
    copy_from({nullptr});
    return *this;
  }

  audited_ptr& operator=(const audited_ptr&) = delete;
  audited_ptr& operator=(audited_ptr&&) = delete;

  const auto use_count() const {
    return mImpl.use_count();
  }

  void dump_refs(std::string_view debugLabel) const {
    const auto refs = mRefData->mRefs;
    dprint("DEBUG: {} references remaining to `{}`", refs.size(), debugLabel);
    for (const auto& [id, loc]: refs) {
      dprint(
        "- {} @ {}:{}:{}",
        loc.function_name(),
        loc.file_name(),
        loc.line(),
        loc.column());
    }
  }

  audited_ptr& copy_from(
    const audited_ptr& other,
    const std::source_location& loc = std::source_location::current()) {
    if (mRefData) {
      mRefData->mRefs.erase(mRefID);
    }

    mImpl = other.mImpl;
    mRefData = other.mRefData;
    mRefID = ~(0ui64);

    if (mRefData) {
      mRefID = mRefData->mNextID++;
      mRefData->mRefs.emplace(mRefID, loc);
    }

    return *this;
  }

  operator bool() const {
    return !!mImpl;
  }
};

}// namespace OpenKneeboard