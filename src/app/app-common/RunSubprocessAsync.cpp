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
#include <OpenKneeboard/RunSubprocessAsync.hpp>
#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>

// clang-format off
#include <shims/Windows.h>
#include <Psapi.h>
#include <shellapi.h>
// clang-format on


namespace OpenKneeboard {
task<SubprocessResult> RunSubprocessAsync(
  std::filesystem::path path,
  std::wstring commandLine,
  RunAs runAs) noexcept {
  if (!std::filesystem::exists(path)) {
    co_return SubprocessResult::DoesNotExist;
  }

  const auto pathStr = path.wstring();

  SHELLEXECUTEINFOW shellExecuteInfo{
    .cbSize = sizeof(SHELLEXECUTEINFOW),
    .fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_NO_CONSOLE,
    .lpVerb = runAs == RunAs::CurrentUser ? L"open" : L"runas",
    .lpFile = pathStr.c_str(),
    .lpParameters = commandLine.c_str(),
  };

  if (!ShellExecuteExW(&shellExecuteInfo)) {
    auto error = GetLastError();
    dprintf("Failed to spawn subprocess: {}", error);
    co_return SubprocessResult::FailedToSpawn;
  }

  if (!shellExecuteInfo.hProcess) {
    dprint("No process handle");
    OPENKNEEBOARD_BREAK;
    co_return SubprocessResult::NoProcessHandle;
  }

  winrt::handle handle{shellExecuteInfo.hProcess};
  const auto exeName = path.filename();
  const auto pid = GetProcessId(handle.get());
  dprintf("Waiting for subprocess '{}' ({})...", exeName, pid);
  co_await winrt::resume_on_signal(handle.get());

  DWORD exitCode{};
  if (!GetExitCodeProcess(handle.get(), &exitCode)) {
    dprintf("Failed to get exit code for process: {}", GetLastError());
    co_return SubprocessResult::NonZeroExit;
  }
  dprintf("Subprocess '{}' ({}) returned {}.", exeName, pid, exitCode);

  if (exitCode == 0) {
    co_return SubprocessResult::Success;
  }
  co_return SubprocessResult::NonZeroExit;
}
}// namespace OpenKneeboard