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
  using caller_type = StackFramePointer;
  uint64_t mRefID {~(0ui64)};

  struct RefData {
    std::unordered_map<uint64_t, caller_type> mRefs;
    uint64_t mNextID {0};

    [[nodiscard]]
    uint64_t AddRef(caller_type caller) {
      const auto id = mNextID++;
      mRefs.emplace(id, caller);
      return id;
    }

    void Release(const uint64_t id) { mRefs.erase(id); }
  };

  std::shared_ptr<T> mImpl;
  std::shared_ptr<felly::guarded_data<RefData>> mRefData;

 public:
  constexpr audited_ptr() = default;
  constexpr audited_ptr(nullptr_t) {}

  audited_ptr(T* ptr, caller_type caller = nullptr) {
    if (!ptr) {
      return;
    }

    if (!caller) {
      caller = StackFramePointer {_ReturnAddress()};
    }
    reset(ptr, caller);
  }

  audited_ptr(const audited_ptr& other, caller_type caller = nullptr) {
    if (!caller) {
      caller = StackFramePointer {_ReturnAddress()};
    }
    copy_from(other, caller);
  }

  audited_ptr& reset(T* ptr = nullptr, caller_type caller = nullptr) {
    if (!caller) {
      caller = StackFramePointer {_ReturnAddress()};
    }

    if (mRefData) {
      mRefData->lock()->Release(std::exchange(mRefID, ~(0ui64)));
    }
    if (!ptr) {
      mRefData.reset();
      mImpl.reset();
      return *this;
    }

    mImpl = std::shared_ptr<T>(ptr);
    RefData refData {};
    mRefID = refData.AddRef(caller);

    mRefData =
      std::make_shared<felly::guarded_data<RefData>>(std::move(refData));
    return *this;
  }

  audited_ptr(audited_ptr&& other) noexcept {
    move_from(std::move(other), StackFramePointer {_ReturnAddress()});
  }

  audited_ptr& operator=(audited_ptr&& other) noexcept {
    move_from(std::move(other), StackFramePointer {_ReturnAddress()});
    return *this;
  }

  audited_ptr& move_from(audited_ptr&& other, caller_type caller = nullptr) {
    if (this == std::addressof(other)) {
      return *this;
    }
    if (mRefData) {
      mRefData->lock()->Release(std::exchange(mRefID, ~(0ui64)));
    }

    if (other.mRefData) {
      other.mRefData->lock()->Release(std::exchange(other.mRefID, ~(0ui64)));
    }

    mImpl = std::move(other.mImpl);
    mRefData = std::move(other.mRefData);
    if (mRefData) {
      if (!caller) {
        caller = StackFramePointer {_ReturnAddress()};
      }
      mRefID = mRefData->lock()->AddRef(caller);
    }
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
    if (mRefData) {
      mRefData->lock()->Release(std::exchange(mRefID, ~(0ui64)));
    }
    mImpl.reset();
    mRefData.reset();
    return *this;
  }

  audited_ptr& operator=(const audited_ptr& other) {
    return copy_from(other, _ReturnAddress());
  }

  auto use_count() const { return mImpl.use_count(); }

  void dump_refs(std::string_view debugLabel) const {
    const auto refData = mRefData->lock();
    const auto& refs = refData->mRefs;
    dprint("DEBUG: {} references remaining to `{}`", refs.size(), debugLabel);
    for (const auto& [id, loc]: refs) {
      dprint(
        "- {} @ {}:{}",
        loc->description(),
        loc->source_file(),
        loc->source_line());
    }
  }

  audited_ptr& copy_from(
    const audited_ptr& other,
    caller_type caller = nullptr) {
    if (mRefData) {
      mRefData->lock()->Release(std::exchange(mRefID, ~(0ui64)));
    }

    mImpl = other.mImpl;
    mRefData = other.mRefData;

    if (mRefData) {
      if (!caller) {
        caller = StackFramePointer {_ReturnAddress()};
      }
      mRefID = mRefData->lock()->AddRef(caller);
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
  audited_ptr<T> lock(StackFramePointer caller = nullptr) {
    auto shared = mImpl.lock();
    if (!shared) {
      return {};
    }
    audited_ptr<T> ret;
    ret.mImpl = std::move(shared);
    ret.mRefData = mRefData;
    if (mRefData) {
      if (!caller) {
        caller = StackFramePointer {_ReturnAddress()};
      }
      ret.mRefID = mRefData->lock()->AddRef(caller);
    }
    return std::move(ret);
  }

 private:
  std::weak_ptr<T> mImpl;
  decltype(audited_ptr<T>::mRefData) mRefData;
};

}// namespace OpenKneeboard
