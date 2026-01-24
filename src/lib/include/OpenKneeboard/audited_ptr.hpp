// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/dprint.hpp>

#include <felly/guarded_data.hpp>

#include <atomic>
#include <source_location>
#include <unordered_map>

namespace OpenKneeboard {

template <class T>
class audited_weak_ptr;

/// Wraps std::shared_ptr to add accounting, to track down leaks
template <class T>
class audited_ptr {
 private:
  uint64_t mRefID {~(0ui64)};

  struct RefData {
    std::unordered_map<uint64_t, std::source_location> mRefs;
    uint64_t mNextID {0};

    [[nodiscard]]
    uint64_t AddRef(const std::source_location& loc) {
      const auto id = mNextID++;
      mRefs.emplace(id, loc);
      return id;
    }

    void Release(const uint64_t id) { mRefs.erase(id); }
  };

  std::shared_ptr<T> mImpl;
  std::shared_ptr<felly::guarded_data<RefData>> mRefData;

 public:
  audited_ptr() = default;
  audited_ptr(nullptr_t) {}

  audited_ptr(
    T* ptr,
    const std::source_location& loc = std::source_location::current()) {
    mImpl = std::shared_ptr<T>(ptr);
    RefData refData {};
    mRefID = refData.AddRef(loc);

    mRefData =
      std::make_shared<felly::guarded_data<RefData>>(std::move(refData));
  }

  audited_ptr(
    const audited_ptr& other,
    const std::source_location& loc = std::source_location::current()) {
    copy_from(other, loc);
  }

  audited_ptr(audited_ptr&&) noexcept = default;
  audited_ptr& operator=(audited_ptr&& other) noexcept {
    if (this == std::addressof(other)) {
      return *this;
    }

    if (mRefData) {
      mRefData->lock()->Release(mRefID);
    }
    mImpl = std::move(other.mImpl);
    mRefData = std::move(other.mRefData);
    mRefID = std::exchange(other.mRefID, ~(0ui64));
    return *this;
  }

  ~audited_ptr() noexcept {
    if (!mRefData) {
      return;
    }
    mRefData->lock()->Release(mRefID);
  }

  T* get() const noexcept { return mImpl.get(); }

  T& operator*() const noexcept { return *mImpl; }

  T* operator->() const noexcept { return mImpl.get(); }

  audited_ptr& operator=(nullptr_t) {
    copy_from({nullptr});
    return *this;
  }

  audited_ptr& operator=(const audited_ptr&) = delete;

  auto use_count() const { return mImpl.use_count(); }

  void dump_refs(std::string_view debugLabel) const {
    const auto refData = mRefData->lock();
    const auto& refs = refData->mRefs;
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
      mRefData->lock()->Release(mRefID);
    }

    mImpl = other.mImpl;
    mRefData = other.mRefData;
    mRefID = ~(0ui64);

    if (mRefData) {
      mRefID = mRefData->lock()->AddRef(loc);
    }

    return *this;
  }

  operator bool() const { return !!mImpl; }

  friend class audited_weak_ptr<T>;
};

template <class T>
class audited_weak_ptr {
 public:
  constexpr audited_weak_ptr() = default;
  audited_weak_ptr(const audited_ptr<T>& ptr)
    : mImpl(ptr.mImpl),
      mRefData(ptr.mRefData) {}
  audited_ptr<T> lock(
    const std::source_location& loc = std::source_location::current()) {
    auto shared = mImpl.lock();
    if (!shared) {
      return {};
    }
    audited_ptr<T> ret;
    ret.mImpl = std::move(shared);
    ret.mRefData = mRefData;
    if (mRefData) {
      ret.mRefID = mRefData->lock()->AddRef(loc);
    }
    return std::move(ret);
  }

 private:
  std::weak_ptr<T> mImpl;
  decltype(audited_ptr<T>::mRefData) mRefData;
};

}// namespace OpenKneeboard
