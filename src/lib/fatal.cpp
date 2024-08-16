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

#include <OpenKneeboard/Win32.hpp>

#include <shims/filesystem>
#include <shims/source_location>
#include <shims/winrt/base.h>

#include <Windows.h>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/filesystem.hpp>
#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/scope_exit.hpp>
#include <OpenKneeboard/version.hpp>

#include <atomic>
#include <bit>
#include <chrono>
#include <fstream>
#include <ranges>

#include <DbgHelp.h>
#include <detours.h>
#include <wchar.h>

using std::operator""s;

namespace {
[[noreturn]]
void fast_fail() {
  __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

struct SkipStacktraceEntries {
  SkipStacktraceEntries() = delete;
  explicit SkipStacktraceEntries(size_t count) : mCount(count) {
  }

  size_t mCount;
};

struct ExceptionRecord {
  std::stacktrace mCreationStack;
};
static thread_local std::optional<ExceptionRecord> tLatestException {};

struct WILFailureRecord {
  std::stacktrace mCreationStack;
  HRESULT mHR {};
  std::wstring mMessage;
  // Used for ERROR_UNHANDLED_EXCEPTION
  std::optional<ExceptionRecord> mException;
};
static thread_local std::optional<WILFailureRecord> tLatestWILFailure {};

}// namespace

template <class CharT>
struct std::formatter<OpenKneeboard::detail::SourceLocation, CharT>
  : std::formatter<std::basic_string_view<CharT>, CharT> {
  template <class FormatContext>
  auto format(
    const OpenKneeboard::detail::SourceLocation& loc,
    FormatContext& fc) const {
    const auto converted = std::format(
      "{}:{}:{} - {}",
      loc.mFileName,
      loc.mLine,
      loc.mColumn,
      loc.mFunctionName);

    return std::formatter<std::basic_string_view<CharT>, CharT>::format(
      std::basic_string_view<CharT> {converted}, fc);
  }
};

namespace OpenKneeboard::detail {

struct CrashMeta {
  // Stack trace with the second entry being the blame frame.
  //
  // This should be a direct stack trace, not a stored/attributed one.
  const std::stacktrace mStacktrace;

  const std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
    mNow {std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now())};
  const DWORD mPID {GetCurrentProcessId()};
  const std::filesystem::path mModulePath {GetModulePath()};

  const std::filesystem::path mCrashLogPath;
  const std::filesystem::path mCrashDumpPath;

  CrashMeta(SkipStacktraceEntries skipCount = SkipStacktraceEntries {0})
    : mStacktrace(std::stacktrace::current(skipCount.mCount + 1)),
      mCrashLogPath(GetCrashFilePath(L"txt")),
      mCrashDumpPath(GetCrashFilePath(L"dmp")) {
  }

  bool CanWriteDump() noexcept {
    this->LoadDbgHelp();
    return static_cast<bool>(mMiniDumpWriteDump);
  }

  decltype(&MiniDumpWriteDump) GetWriteMiniDumpProc() {
    this->LoadDbgHelp();
    if (!mMiniDumpWriteDump) {
      OPENKNEEBOARD_BREAK;
      fast_fail();
    }
    return mMiniDumpWriteDump;
  }

 private:
  inline static std::filesystem::path GetModulePath() {
    HMODULE thisModule {nullptr};
    GetModuleHandleExA(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      std::bit_cast<LPCSTR>(_ReturnAddress()),
      &thisModule);
    wchar_t buf[MAX_PATH];
    const auto charCount = GetModuleFileNameW(thisModule, buf, MAX_PATH);
    return {std::wstring_view {buf, charCount}};
  }

  std::filesystem::path GetCrashFilePath(std::wstring_view extension) {
    return Filesystem::GetLogsDirectory()
      / std::format(
             L"{}-crash-{:%Y%m%dT%H%M%S}-{}.{}",
             mModulePath.stem(),
             mNow,
             mPID,
             extension);
  }

  unique_hmodule mDbgHelp;
  decltype(&MiniDumpWriteDump) mMiniDumpWriteDump {nullptr};
  bool mLoadedDbgHelp {false};
  void LoadDbgHelp() {
    if (std::exchange(mLoadedDbgHelp, true)) {
      return;
    }
    mDbgHelp.reset(LoadLibraryW(L"Dbghelp.dll"));
    if (!mDbgHelp) {
      return;
    }

    mMiniDumpWriteDump = reinterpret_cast<decltype(&MiniDumpWriteDump)>(
      GetProcAddress(mDbgHelp.get(), "MiniDumpWriteDump"));
  }
};

enum class DumpType {
  MiniDump,
  FullDump,
};

static void CreateDump(
  CrashMeta& meta,
  DumpType dumpType,
  std::string_view extraData,
  LPEXCEPTION_POINTERS exceptionPointers) {
  static std::atomic_flag sDumped;
  if (sDumped.test_and_set()) {
    return;
  }

  if (!meta.CanWriteDump()) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  const auto dumpFile = Win32::CreateFileW(
    meta.mCrashDumpPath.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    0,
    nullptr,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  MINIDUMP_EXCEPTION_INFORMATION exceptionInfo {
    .ThreadId = GetCurrentThreadId(),
    .ExceptionPointers = exceptionPointers,
    .ClientPointers /* exception in debugger target */ = false,
  };

  EXCEPTION_RECORD exceptionRecord {};
  CONTEXT exceptionContext {};
  EXCEPTION_POINTERS fakeExceptionPointers;
  if (!exceptionPointers) {
    ::RtlCaptureContext(&exceptionContext);
    exceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
    fakeExceptionPointers = {&exceptionRecord, &exceptionContext};
    exceptionInfo.ExceptionPointers = &fakeExceptionPointers;
  }

  constexpr auto miniDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData
    | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo);
  constexpr auto fullDumpType = static_cast<MINIDUMP_TYPE>(
    (miniDumpType & ~MiniDumpWithIndirectlyReferencedMemory)
    | MiniDumpWithFullMemory | MiniDumpIgnoreInaccessibleMemory);
  auto thisDumpType
    = (dumpType == DumpType::FullDump) ? fullDumpType : miniDumpType;

  (*meta.GetWriteMiniDumpProc())(
    GetCurrentProcess(),
    meta.mPID,
    dumpFile.get(),
    thisDumpType,
    &exceptionInfo,
    /* user stream = */ nullptr,
    /* callback = */ nullptr);
}

[[msvc::noinline]]
static std::string GetFatalLogContents(
  const CrashMeta& meta,
  const FatalData& fatal) noexcept {
  static std::atomic_flag sRecursing;
  if (sRecursing.test_and_set()) {
    OutputDebugStringA(
      std::format("💀💀 FATAL DURING FATAL: {}", fatal.mMessage).c_str());
    fast_fail();
  }

  std::string blameString;
  if (fatal.mBlameLocation) {
    blameString = std::format("{}", *fatal.mBlameLocation);
  } else {
    const auto& caller = meta.mStacktrace.at(1);
    blameString = std::format(
      "{}:{} - {}",
      caller.source_file(),
      caller.source_line(),
      caller.description());
  }

  // Let's just get the basics out early in case anything else goes wrong
  dprintf("💀 FATAL: {} @ {}", fatal.mMessage, blameString);

  std::stringstream f;

  const auto executable = Filesystem::GetCurrentExecutablePath();

  f << std::format(
    "{} (PID {}) crashed at {:%Y%m%dT%H%M%S}\n\n",
    executable.filename(),
    meta.mPID,
    meta.mNow)
    << std::format("💀 FATAL: {}\n", fatal.mMessage);

  std::unique_ptr<wchar_t, decltype(&LocalFree)> threadDescriptionBuf {
    nullptr, &LocalFree};
  GetThreadDescription(GetCurrentThread(), std::out_ptr(threadDescriptionBuf));
  const auto threadDescription = winrt::to_string(threadDescriptionBuf.get());

  f << "\n"
    << "Metadata\n"
    << "========\n\n"
    << std::format("Executable:  {}\n", executable)
    << std::format("Module:      {}\n", meta.mModulePath)
    << std::format(
         "Thread:      {} {}\n",
         std::this_thread::get_id(),
         threadDescription.empty() ? "[no description]"s
                                   : std::format("(\"{}\")", threadDescription))
    << std::format("Blame frame: {}\n", blameString)
    << std::format("OKB Version: {}\n", Version::ReleaseName);

  f << "\n"
    << "Stack Trace\n"
    << "===========\n\n"
    << meta.mStacktrace << "\n";

  if (tLatestException) {
    f << "\n"
      << "Latest Exception\n"
      << "================\n\n"
      << tLatestException->mCreationStack << "\n";
  }

  if (tLatestWILFailure) {
    f << "\n"
      << "Latest WIL Failure\n"
      << "==================\n"
      << "\n"
      << std::format(
           "HRESULT: {:#018x} ({})\n",
           static_cast<uint32_t>(tLatestWILFailure->mHR),
           winrt::to_string(
             winrt::hresult_error(tLatestWILFailure->mHR).message()))
      << std::format(
           "Message: {}\n", winrt::to_string(tLatestWILFailure->mMessage));

    if (tLatestWILFailure->mException) {
      f << std::format(
        "\nException:\n\n{}\n", tLatestWILFailure->mException->mCreationStack);
    }
  }

  auto dumper = GetDPrintDumper();
  if (dumper) {
    f << "\n"
      << "Logs\n"
      << "====\n\n"
      << dumper() << "\n";
  }

  return f.str();
}

[[noreturn, msvc::noinline]] static void FatalAndDump(
  CrashMeta& meta,
  const FatalData& fatal,
  LPEXCEPTION_POINTERS dumpableExceptions) {
  const auto logContents = GetFatalLogContents(meta, fatal);

  {
    std::ofstream f(meta.mCrashLogPath, std::ios::binary);
    f << logContents;
  }

  if (IsDebuggerPresent()) {
    __debugbreak();
  }

  if (meta.CanWriteDump()) {
    auto thisProcess = Filesystem::GetCurrentExecutablePath();

    const auto createFullDump = MessageBox(
      NULL,
      L"OpenKneeboard has crashed and a log has been created; would you like "
      L"to "
      L"create a full dump that you can share with the developers?\n\nThis "
      L"may create a file that is several hundred megabytes.",
      thisProcess.stem().c_str(),
      MB_YESNO | MB_ICONERROR | MB_TASKMODAL);

    if (createFullDump == IDYES) {
      CreateDump(meta, DumpType::FullDump, logContents, dumpableExceptions);

      try {
        Filesystem::OpenExplorerWithSelectedFile(meta.mCrashDumpPath);
      } catch (...) {
      }

      fast_fail();
    }

    const auto createMiniDump = MessageBox(
      NULL,
      L"Would you like to create a minidump instead?\n\nThis will usually be "
      L"tens of megabytes.",
      thisProcess.stem().c_str(),
      MB_YESNO | MB_ICONERROR | MB_TASKMODAL);
    if (createMiniDump == IDYES) {
      CreateDump(meta, DumpType::MiniDump, logContents, dumpableExceptions);

      try {
        Filesystem::OpenExplorerWithSelectedFile(meta.mCrashDumpPath);
      } catch (...) {
      }

      fast_fail();
    }
  }

  try {
    Filesystem::OpenExplorerWithSelectedFile(meta.mCrashLogPath);
  } catch (...) {
  }

  fast_fail();
}

void FatalData::fatal() const noexcept {
  CrashMeta meta {SkipStacktraceEntries {1}};
  FatalAndDump(meta, *this, nullptr);
}

struct WILResultInfo {
  wil::FailureInfo mFailureInfo {};
  std::wstring mDebugMessage;
};

static void OnTerminate() {
  fatal("std::terminate() called");
}

LONG __callback WINAPI
OnUnhandledException(LPEXCEPTION_POINTERS exceptionPointers) {
  CrashMeta meta {};
  FatalAndDump(meta, {"Uncaught exceptions"}, exceptionPointers);
  return EXCEPTION_EXECUTE_HANDLER;
}

static void __stdcall OnWILResult(
  wil::FailureInfo* failure,
  PWSTR debugMessage,
  size_t debugMessageChars) noexcept {
  tLatestWILFailure = {
    .mCreationStack = std::stacktrace::current(),
    .mHR = failure->hr,
    .mMessage
    = std::wstring {debugMessage, wcsnlen_s(debugMessage, debugMessageChars)},
  };
  if (
    failure->hr == HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION)
    && tLatestException) {
    tLatestWILFailure->mException = *tLatestException;
  }
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

static decltype(&_CxxThrowException) gCxxThrowException {&_CxxThrowException};
extern "C" void __stdcall CxxThrowExceptionHook(
  void* pExceptionObject,
  _ThrowInfo* pThrowInfo) {
  if (pExceptionObject) {
    // Otherwise, it's a rethrow
    tLatestException = {
      std::stacktrace::current(1),
    };
  }

  gCxxThrowException(pExceptionObject, pThrowInfo);
}

}// namespace OpenKneeboard::detail

namespace OpenKneeboard {

void fatal_with_hresult(HRESULT hr) {
  using namespace OpenKneeboard::detail;
  CrashMeta meta {};
  FatalAndDump(
    meta,
    {std::format(
      "HRESULT {:#018x} ({})",
      static_cast<uint32_t>(hr),
      winrt::to_string(winrt::hresult_error(hr).message()))},
    nullptr);
}

void divert_process_failure_to_fatal() {
  using namespace OpenKneeboard::detail;

  wil::SetResultMessageCallback(&OnWILResult);

  // What could go wrong
  winrt::check_win32(DetourTransactionBegin());
  winrt::check_win32(DetourAttach(&gCxxThrowException, &CxxThrowExceptionHook));
  winrt::check_win32(DetourTransactionCommit());

  divert_thread_failure_to_fatal();
  gDivertThreadFailureToFatal = true;
}

}// namespace OpenKneeboard