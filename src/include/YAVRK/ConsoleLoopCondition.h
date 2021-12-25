#pragma once

#include <chrono>
#include <memory>

namespace YAVRK {

class ConsoleLoopCondition final {
 public:
  enum class EventType { SLEEP, EXIT };
  ConsoleLoopCondition();
  ~ConsoleLoopCondition();

  EventType waitForSleepOrExit(
    const std::chrono::steady_clock::duration& delay);

 private:
  class Impl;
  std::shared_ptr<Impl> p;
};

}// namespace YAVRK
