/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <shims/winrt/base.h>

#include <source_location>

namespace OpenKneeboard {

/** Block process shutdown until it has been destroyed.
 *
 * Some objects have async cleanup, e.g.
 * - co_await to be destroyed on the correct thread
 * - co_await'ing windows runtime IAsyncActions
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