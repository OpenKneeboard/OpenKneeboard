// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <functional>
#include <mutex>

namespace OpenKneeboard {
template <class T>
class LazyOnceValue {
 public:
  LazyOnceValue() = delete;
  LazyOnceValue(std::function<T()> f) : mFunc(f) {
  }

  operator T() noexcept {
    std::call_once(mOnce, [this]() { mValue = mFunc(); });
    return mValue;
  }

 private:
  std::function<T()> mFunc;
  std::once_flag mOnce;
  T mValue;
};

}