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

#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/handles.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <atomic>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <stacktrace>

#include <DbgHelp.h>
#include <wchar.h>

#ifndef __clang__
#include <detours.h>
#endif

#ifdef __clang__
// Microsoft C++ intrinsics
using _ThrowInfo = void*;
extern "C" void __stdcall _CxxThrowException(void*, _ThrowInfo);
#endif

using std::operator""s;

namespace {
[[noreturn]] void fast_fail() {
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

MINIDUMP_TYPE gMinidumpType {MiniDumpNormal};
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
  std::stacktrace mStacktrace;

  std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
    mNow {std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now())};
  DWORD mPID {GetCurrentProcessId()};
  std::filesystem::path mModulePath {GetModulePath()};

  std::filesystem::path mCrashLogPath;
  std::filesystem::path mCrashDumpPath;

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
    return Filesystem::GetCrashLogsDirectory()
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

static void CreateDump(
  CrashMeta& meta,
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

  const auto dumpFile = Win32::or_default::CreateFile(
    meta.mCrashDumpPath.c_str(),
    GENERIC_READ | GENERIC_WRITE,
    0,
    nullptr,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL);
  if (!dumpFile) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  MINIDUMP_EXCEPTION_INFORMATION exceptionInfo {
    .ThreadId = GetCurrentThreadId(),
    .ExceptionPointers = exceptionPointers,
    .ClientPointers /* exception in debugger target */ = false,
  };

  EXCEPTION_RECORD exceptionRecord {};
  CONTEXT exceptionContext {
    .ContextFlags = CONTEXT_ALL,
  };
  EXCEPTION_POINTERS fakeExceptionPointers;
  if (!exceptionPointers) {
    ::RtlCaptureContext(&exceptionContext);
    exceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
    fakeExceptionPointers = {&exceptionRecord, &exceptionContext};
    exceptionInfo.ExceptionPointers = &fakeExceptionPointers;
  }

  MINIDUMP_USER_STREAM_INFORMATION userStreams {};
  MINIDUMP_USER_STREAM extraDataStream {};

  std::wstring extraDataW;
  if (!extraData.empty()) {
    extraDataW = winrt::to_hstring(extraData);
  }

  if (!extraData.empty()) {
    extraDataStream = {
      .Type = CommentStreamW,
      .BufferSize
      = static_cast<ULONG>(extraDataW.size() * sizeof(extraDataW[0])),
      .Buffer
      = const_cast<void*>(reinterpret_cast<const void*>(extraDataW.data())),
    };
    userStreams = {
      .UserStreamCount = 1,
      .UserStreamArray = &extraDataStream,
    };
  }

  (*meta.GetWriteMiniDumpProc())(
    GetCurrentProcess(),
    meta.mPID,
    dumpFile.get(),
    gMinidumpType,
    &exceptionInfo,
    &userStreams,
    /* callback = */
    nullptr);
}

SourceLocation::SourceLocation(const std::stacktrace_entry& entry) noexcept {
  if (!entry) [[unlikely]] {
    dprint(
      "Attempted to construct a SourceLocation with an empty stacktrace_entry");
    OPENKNEEBOARD_BREAK;
    return;
  }
  mFunctionName = entry.description();
  mFileName = entry.source_file();
  mLine = entry.source_line();
}

SourceLocation::SourceLocation(const std::source_location& loc) noexcept
  : mFunctionName(loc.function_name()),
    mFileName(loc.file_name()),
    mLine(loc.line()),
    mColumn(loc.column()) {
}

SourceLocation::SourceLocation(StackFramePointer frame) noexcept
  : SourceLocation(std::bit_cast<std::stacktrace_entry>(frame.mValue)) {
}

OPENKNEEBOARD_NOINLINE
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
  dprint("💀 FATAL: {} @ {}", fatal.mMessage, blameString);

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

  const auto dprintHistory = dprint.MaybeGetHistory();
  if (dprintHistory) {
    f << "\n"
      << "Logs\n"
      << "====\n\n"
      << *dprintHistory << "\n";
  }

  return f.str();
}

OPENKNEEBOARD_NOINLINE
[[noreturn]] static void FatalAndDump(
  CrashMeta& meta,
  const FatalData& fatal,
  LPEXCEPTION_POINTERS dumpableExceptions) {
  const auto logContents = GetFatalLogContents(meta, fatal);

  {
    std::ofstream f(meta.mCrashLogPath, std::ios::binary);
    f << logContents;
  }

  if (meta.CanWriteDump()) {
    auto thisProcess = Filesystem::GetCurrentExecutablePath();

    CreateDump(meta, logContents, dumpableExceptions);

    try {
      Filesystem::OpenExplorerWithSelectedFile(meta.mCrashDumpPath);
    } catch (...) {
    }

    fast_fail();
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

void SetDumpType(DumpType type) {
  constexpr auto miniDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData
    | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo);
  constexpr auto fullDumpType = static_cast<MINIDUMP_TYPE>(
    (miniDumpType & ~MiniDumpWithIndirectlyReferencedMemory)
    | MiniDumpWithFullMemory | MiniDumpIgnoreInaccessibleMemory);
  switch (type) {
    case DumpType::MiniDump:
      gMinidumpType = miniDumpType;
      break;
    case DumpType::FullDump:
      gMinidumpType = fullDumpType;
      break;
  }
}

void fatal_with_hresult(HRESULT hr) {
  using namespace OpenKneeboard::detail;
  prepare_to_fatal();
  CrashMeta meta {};
  FatalAndDump(
    meta,
    {std::format(
      "HRESULT {:#018x} ({})",
      static_cast<uint32_t>(hr),
      winrt::to_string(winrt::hresult_error(hr).message()))},
    nullptr);
}

void fatal_with_exception(std::exception_ptr ep) {
  using namespace OpenKneeboard::detail;
  prepare_to_fatal();
  CrashMeta meta {};
  if (!ep) {
    FatalAndDump(
      meta, {"fatal_with_exception() called without an exception"}, nullptr);
    std::unreachable();
  }

  try {
    std::rethrow_exception(std::current_exception());
  } catch (const winrt::hresult_error& e) {
    FatalAndDump(
      meta,
      {
        std::format(
          "Uncaught winrt::hresult_error: {:#018x} - {}",
          static_cast<uint32_t>(e.code()),
          winrt::to_string(e.message())),
      },
      nullptr);
  } catch (const std::exception& e) {
    FatalAndDump(
      meta,
      {
        std::format(
          "Uncaught std::exception ({}): {}", typeid(e).name(), e.what()),
      },
      nullptr);
    std::unreachable();
  }

  FatalAndDump(meta, {"Uncaught exception of unknown kind"}, nullptr);
}

void divert_process_failure_to_fatal() {
  using namespace OpenKneeboard::detail;

  wil::SetResultMessageCallback(&OnWILResult);

#ifndef __clang__
  // What could go wrong
  winrt::check_win32(DetourTransactionBegin());
  winrt::check_win32(DetourAttach(&gCxxThrowException, &CxxThrowExceptionHook));
  winrt::check_win32(DetourTransactionCommit());
#endif

  divert_thread_failure_to_fatal();
  gDivertThreadFailureToFatal = true;
}

std::string StackFramePointer::to_string() const noexcept {
  if (!mValue) {
    return "[nullptr]";
  }

  return std::format("{}", std::bit_cast<std::stacktrace_entry>(*this));
}
}// namespace OpenKneeboard