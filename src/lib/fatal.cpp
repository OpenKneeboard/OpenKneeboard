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
#include <shims/winrt/base.h>

#include <Windows.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/filesystem.hpp>
#include <OpenKneeboard/version.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <ranges>

#include <wchar.h>

using std::operator""s;

namespace OpenKneeboard::detail {

[[noreturn]]
static void fast_fail() {
  // The FAST_FAIL_FATAL_APP_EXIT macro is defined in winnt.h, but we don't want
  // to pull that in here
  constexpr unsigned int fast_fail_fatal_app_exit = 7;
  __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

static void log_fatal(const FatalData& fatal) noexcept {
  static std::atomic_flag sRecursing;
  if (sRecursing.test_and_set()) {
    OutputDebugStringA(
      std::format("💀💀 FATAL DURING FATAL: {}", fatal.mMessage).c_str());
    OPENKNEEBOARD_BREAK;
    fast_fail();
  }

  const auto actualTrace = std::stacktrace::current(-3);
  std::string blameString;
  if (fatal.mBlameLocation) {
    blameString = std::format("{}", *fatal.mBlameLocation);
  } else {
    const auto& caller = actualTrace.at(2);
    blameString = std::format(
      "{}:{} - {}",
      caller.source_file(),
      caller.source_line(),
      caller.description());
  }

  // Let's just get the basics out early in case anything else goes wrong
  dprintf("💀 FATAL: {} @ {}", fatal.mMessage, blameString);

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
    << std::format("💀 FATAL: {}\n", fatal.mMessage);

  std::unique_ptr<wchar_t, decltype(&LocalFree)> threadDescriptionBuf {
    nullptr, &LocalFree};
  GetThreadDescription(GetCurrentThread(), std::out_ptr(threadDescriptionBuf));
  const auto threadDescription = winrt::to_string(threadDescriptionBuf.get());

  f << "\n"
    << "Metadata\n"
    << "========\n\n"
    << std::format("Executable:  {}\n", executable)
    << std::format("Module:      {}\n", modulePath)
    << std::format(
         "Thread:      {} {}\n",
         std::this_thread::get_id(),
         threadDescription.empty() ? "[no description]"s
                                   : std::format("(\"{}\")", threadDescription))
    << std::format("Blame frame: {}\n", blameString)
    << std::format("OKB Version: {}\n", Version::ReleaseName);

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
}

void FatalData::fatal() const noexcept {
  log_fatal(*this);
  OPENKNEEBOARD_BREAK;
  fast_fail();
}

struct WILResultInfo {
  wil::FailureInfo mFailureInfo {};
  std::wstring mDebugMessage;
};

static thread_local WILResultInfo tLastWILResult {};

static void OnTerminate() {
  __debugbreak();
  if (!std::uncaught_exceptions()) {
    if (tLastWILResult.mFailureInfo.returnAddress) {
      const auto trace = std::stacktrace::current();
      auto it = std::ranges::find_if(
        trace,
        [&info = tLastWILResult.mFailureInfo](const std::stacktrace_entry& it) {
          return (it.source_file() == info.pszFile)
            && (it.source_line() == info.uLineNumber);
        });
      __debugbreak();
    }
    fatal("std::terminate() called with no uncaught exceptions");
  }

  try {
    std::rethrow_exception(std::current_exception());
  } catch (const winrt::hresult_error& e) {
    fatal(
      "std::terminate() called with uncaught winrt::hresult_error: {}",
      winrt::to_string(e.message()));
  } catch (const std::exception& e) {
    fatal(
      "std::terminate() called with derived_from<std::exception> ({}): {}",
      typeid(e).name(),
      e.what());
  } catch (...) {
    fatal(
      "std::terminate() called with an exception with an unknown base class");
  }
}

LONG __callback WINAPI
OnUnhandledException(LPEXCEPTION_POINTERS exceptionPointers) {
  auto count = std::uncaught_exceptions();
  __debugbreak();
  return EXCEPTION_EXECUTE_HANDLER;
}

static void __stdcall OnWILResult(
  wil::FailureInfo* failure,
  PWSTR debugMessage,
  size_t debugMessageChars) noexcept {
  tLastWILResult = {
    *failure,
    std::wstring {debugMessage, wcsnlen_s(debugMessage, debugMessageChars)}};
}

static void divert_thread_failure_to_fatal() {
  std::set_terminate(&OnTerminate);
  SetUnhandledExceptionFilter(&OnUnhandledException);
}

static bool gDivertThreadFailureToFatal {false};

struct ThreadFailureHook {
  ThreadFailureHook() {
    if (gDivertThreadFailureToFatal) {
      divert_thread_failure_to_fatal();
    }
  }
};
static thread_local ThreadFailureHook tThreadFailureHook;

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

void divert_process_failure_to_fatal() {
  wil::SetResultMessageCallback(&detail::OnWILResult);

  detail::divert_thread_failure_to_fatal();
  detail::gDivertThreadFailureToFatal = true;
}

}// namespace OpenKneeboard