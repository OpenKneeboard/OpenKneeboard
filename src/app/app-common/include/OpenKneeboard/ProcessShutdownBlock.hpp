// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

#include <source_location>

namespace OpenKneeboard {

/** Block process shutdown until it has been destroyed.
 *
 * Some objects have async cleanup, e.g.
 * - co_await to be destroyed on the correct thread
 * - co_await'ing windows runtime task<void>s
 *
 * C++/WinRT's `final_release()` pattern is used for these,
 * but we need a way to wait for those all to finish, even
 * as they jump between threads etc.
 */
class ProcessShutdownBlock {
 public:
  ProcessShutdownBlock(
    const std::source_location& loc = std::source_location::current());
  ~ProcessShutdownBlock();

  ProcessShutdownBlock(const ProcessShutdownBlock&) = delete;
  ProcessShutdownBlock(ProcessShutdownBlock&&) = delete;
  ProcessShutdownBlock& operator=(const ProcessShutdownBlock&) = delete;
  ProcessShutdownBlock& operator=(ProcessShutdownBlock&&) = delete;

  static void SetEventOnCompletion(HANDLE completionEvent) noexcept;
  static void DumpActiveBlocks() noexcept;

 private:
  uint64_t mID {~(0ui64)};
};

}// namespace OpenKneeboard
