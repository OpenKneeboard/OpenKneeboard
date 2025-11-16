// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.

#include <OpenKneeboard/EnumerateProcesses.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/tracing.hpp>

namespace OpenKneeboard {

ProcessList::ProcessList() : ProcessList(nullptr, 0) {
}

ProcessList::ProcessList(element_type* list, DWORD count)
  : mList(list), mCount(count) {
}

ProcessList::ProcessList(ProcessList&& other) noexcept {
  *this = std::move(other);
}

ProcessList& ProcessList::operator=(ProcessList&& other) noexcept {
  this->Release();
  mList = std::exchange(other.mList, nullptr);
  mCount = std::exchange(other.mCount, 0);
  return *this;
}

ProcessList::~ProcessList() {
  this->Release();
}

void ProcessList::Release() {
  if (!mList) {
    return;
  }
  if (!WTSFreeMemoryExW(WTSTypeProcessInfoLevel1, mList, mCount)) {
    __debugbreak();
  }
  mList = nullptr;
  mCount = 0;
}

std::expected<ProcessList, HRESULT> EnumerateProcesses() {
  ProcessList ret;
  DWORD level {1};
  while (true) {
    ret.Release();

    OPENKNEEBOARD_TraceLoggingScope("WTSEnumerateProcessesExW()");
    if (WTSEnumerateProcessesExW(
          WTS_CURRENT_SERVER_HANDLE,
          &level,
          WTS_CURRENT_SESSION,
          reinterpret_cast<LPWSTR*>(&ret.mList),
          &ret.mCount)) {
      break;
    }
    const auto code = GetLastError();
    if (code == ERROR_BAD_LENGTH) {
      continue;// ?! We're not providing a length...
    }
    dprint.Error("WTSEnumerateProcessesExW() failed with {}", code);
    return std::unexpected {HRESULT_FROM_WIN32(code)};
  }

  return ret;
}

}// namespace OpenKneeboard