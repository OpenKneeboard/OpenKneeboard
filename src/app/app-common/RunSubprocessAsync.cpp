// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/RunSubprocessAsync.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>

#include <Windows.h>
#include <Psapi.h>
#include <shellapi.h>

namespace OpenKneeboard {
task<SubprocessResult> RunSubprocessAsync(
  std::filesystem::path path,
  std::wstring commandLine,
  RunAs runAs) noexcept {
  if (!std::filesystem::exists(path)) {
    co_return SubprocessResult::DoesNotExist;
  }

  const auto pathStr = path.wstring();

  SHELLEXECUTEINFOW shellExecuteInfo {
    .cbSize = sizeof(SHELLEXECUTEINFOW),
    .fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_NO_CONSOLE,
    .lpVerb = runAs == RunAs::CurrentUser ? L"open" : L"runas",
    .lpFile = pathStr.c_str(),
    .lpParameters = commandLine.c_str(),
  };

  if (!ShellExecuteExW(&shellExecuteInfo)) {
    auto error = GetLastError();
    dprint("Failed to spawn subprocess: {}", error);
    co_return SubprocessResult::FailedToSpawn;
  }

  if (!shellExecuteInfo.hProcess) {
    dprint("No process handle");
    OPENKNEEBOARD_BREAK;
    co_return SubprocessResult::NoProcessHandle;
  }

  winrt::handle handle {shellExecuteInfo.hProcess};
  const auto exeName = path.filename();
  const auto pid = GetProcessId(handle.get());
  dprint("Waiting for subprocess '{}' ({})...", exeName, pid);
  co_await winrt::resume_on_signal(handle.get());

  DWORD exitCode {};
  if (!GetExitCodeProcess(handle.get(), &exitCode)) {
    dprint("Failed to get exit code for process: {}", GetLastError());
    co_return SubprocessResult::NonZeroExit;
  }
  dprint("Subprocess '{}' ({}) returned {}.", exeName, pid, exitCode);

  if (exitCode == 0) {
    co_return SubprocessResult::Success;
  }
  co_return SubprocessResult::NonZeroExit;
}
}// namespace OpenKneeboard