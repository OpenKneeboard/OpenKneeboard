// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <Windows.h>
#include <wtsapi32.h>

#include <expected>

namespace OpenKneeboard {

class ProcessList final {
 public:
  using element_type = WTS_PROCESS_INFO_EXW;
  ProcessList();
  ProcessList(element_type* list, DWORD count);
  ~ProcessList();

  ProcessList(ProcessList&&) noexcept;
  ProcessList& operator=(ProcessList&&) noexcept;

  ProcessList(const ProcessList&) = delete;
  ProcessList& operator=(const ProcessList&) = delete;

  friend constexpr element_type* begin(const ProcessList&) noexcept;
  friend constexpr element_type* end(const ProcessList&) noexcept;
  friend std::expected<ProcessList, HRESULT> EnumerateProcesses();

 private:
  element_type* mList {nullptr};
  DWORD mCount {};

  void Release();
};

constexpr ProcessList::element_type* begin(const ProcessList& list) noexcept {
  return list.mList;
}
constexpr ProcessList::element_type* end(const ProcessList& list) noexcept {
  if (!list.mList) {
    return nullptr;
  }
  return &list.mList[list.mCount];
}

constexpr ProcessList::element_type* begin(
  const std::expected<ProcessList, HRESULT>& list) noexcept {
  if (!list) {
    return nullptr;
  }
  return begin(list.value());
}

constexpr ProcessList::element_type* end(
  const std::expected<ProcessList, HRESULT>& list) noexcept {
  if (!list) {
    return nullptr;
  }
  return end(list.value());
}

std::expected<ProcessList, HRESULT> EnumerateProcesses();

}// namespace OpenKneeboard
