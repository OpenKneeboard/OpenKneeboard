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

#include <shims/Windows.h>

#include <memory>

namespace OpenKneeboard {

template <class T>
struct Handle {
  T mHandle {};
  Handle() = default;
  Handle(T handle) : mHandle(handle) {
  }
  Handle(nullptr_t) {
  }

  operator bool() const noexcept {
    return static_cast<bool>(mHandle);
  }

  operator T() const {
    return mHandle;
  }

  bool operator==(const std::nullptr_t&) const noexcept {
    return !mHandle;
  }

  auto operator<=>(const Handle<T>&) const noexcept = default;
};

template <class TPtr, auto TDeleter>
struct CDeleter {
  using pointer = TPtr;
  // Auto allows implicit cast to void* where appropriate
  void operator()(auto v) const {
    if (v) {
      TDeleter(v);
    }
  }
};

template <class T, auto TDeleter>
using CHandleDeleter = CDeleter<Handle<T>, TDeleter>;
template <class T, auto TDeleter>
using CPtrDeleter = CDeleter<T*, TDeleter>;

template <class T>
using unique_co_task_ptr = std::unique_ptr<T, CPtrDeleter<T, &CoTaskMemFree>>;

using unique_hwineventhook = std::
  unique_ptr<HWINEVENTHOOK, CHandleDeleter<HWINEVENTHOOK, &UnhookWinEvent>>;
using unique_hhook
  = std::unique_ptr<HHOOK, CHandleDeleter<HHOOK, &UnhookWindowsHookEx>>;
using unique_hmodule
  = std::unique_ptr<HMODULE, CHandleDeleter<HMODULE, &FreeLibrary>>;

}// namespace OpenKneeboard
