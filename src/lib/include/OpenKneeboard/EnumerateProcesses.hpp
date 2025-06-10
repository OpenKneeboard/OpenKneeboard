/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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