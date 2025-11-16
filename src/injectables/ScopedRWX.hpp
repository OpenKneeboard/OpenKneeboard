// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <memory>

namespace OpenKneeboard {
class ScopedRWX final {
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  ScopedRWX(void* addr);
  ~ScopedRWX();
};
}// namespace OpenKneeboard
