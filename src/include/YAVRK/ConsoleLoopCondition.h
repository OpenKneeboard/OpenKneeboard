#pragma once

#include <chrono>
#include <memory>

namespace YAVRK {

class ConsoleLoopCondition final {
 public:
  ConsoleLoopCondition();
  ~ConsoleLoopCondition();

  /// Returns false if sleep is interrupted
  bool sleep(const std::chrono::steady_clock::duration& delay);

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

}// namespace YAVRK
