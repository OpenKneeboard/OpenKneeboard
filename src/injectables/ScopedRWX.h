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
