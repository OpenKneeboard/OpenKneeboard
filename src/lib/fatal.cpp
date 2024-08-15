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

#include <shims/filesystem>
#include <shims/source_location>

#include <Windows.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/filesystem.hpp>
#include <OpenKneeboard/version.hpp>

#include <atomic>
#include <chrono>
#include <fstream>

namespace OpenKneeboard::detail {

void FatalData::fatal() const noexcept {
  static std::atomic_flag sRecursing;
  if (sRecursing.test_and_set()) {
    OutputDebugStringA(
      std::format("💀💀 FATAL DURING FATAL: {}", mMessage).c_str());
    OPENKNEEBOARD_BREAK;
    std::terminate();
  }

  const auto actualTrace = std::stacktrace::current(-2);
  std::string blameString;
  if (mBlameLocation) {
    blameString = std::format("{}", *mBlameLocation);
  } else {
    const auto& caller = actualTrace.at(2);
    blameString = std::format(
      "{}:{} - {}",
      caller.source_file(),
      caller.source_line(),
      caller.description());
  }

  // Let's just get the basics out early in case anything else goes wrong
  dprintf("💀 FATAL: {} @ {}", mMessage, blameString);

  HMODULE thisModule {nullptr};
  GetModuleHandleExA(
    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
      | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    std::bit_cast<LPCSTR>(&FatalData::fatal),
    &thisModule);
  wchar_t moduleNameBuf[MAX_PATH];
  const auto moduleNameChars
    = GetModuleFileNameW(thisModule, moduleNameBuf, MAX_PATH);
  const std::filesystem::path modulePath {
    std::wstring_view {moduleNameBuf, moduleNameChars}};

  const std::filesystem::path logsDirectory = Filesystem::GetLogsDirectory();
  const auto now = std::chrono::time_point_cast<std::chrono::seconds>(
    std::chrono::system_clock::now());
  const auto pid = GetCurrentProcessId();

  const auto crashFile
    = logsDirectory
    / std::format(
        L"{}-crash-{:%Y%m%dT%H%M%S}-{}.txt", modulePath.stem(), now, pid);

  std::ofstream f(crashFile, std::ios::binary);

  const auto executable = Filesystem::GetCurrentExecutablePath();

  f << std::format(
    "{} (PID {}) crashed at {:%Y%m%dT%H%M%S}\n\n",
    executable.filename(),
    pid,
    now)
    << std::format("💀 FATAL: {}\n", mMessage);

  f << "\n"
    << "Metadata\n"
    << "========\n\n"
    << std::format("Executable:  {}\n", executable)
    << std::format("Module:      {}\n", modulePath)
    << std::format("Blame frame: {}\n", blameString)
    << std::format("OKB Version: {}\n", Version::ReleaseName);

  f << "\n"
    << "Provided trace\n"
    << "==============\n\n";
  if (mBlameTrace) {
    f << *mBlameTrace << "\n";
  } else {
    f << "None.\n";
  }

  f << "\n"
    << "Actual trace\n"
    << "============\n\n"
    << actualTrace << "\n";

  auto dumper = GetDPrintDumper();
  if (dumper) {
    f << "\n"
      << "Logs\n"
      << "====\n\n"
      << dumper() << "\n";
  }

  f.close();

  Filesystem::OpenExplorerWithSelectedFile(crashFile);

  OPENKNEEBOARD_BREAK;
  std::terminate();
}

}// namespace OpenKneeboard::detail