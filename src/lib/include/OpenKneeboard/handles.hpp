// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <Windows.h>
#include <objbase.h>

#include <memory>

namespace OpenKneeboard {

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
using CHandleDeleter = CDeleter<T, TDeleter>;
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
using unique_hkey = std::unique_ptr<HKEY, CHandleDeleter<HKEY, &RegCloseKey>>;
using unique_hwnd = std::unique_ptr<HWND, CHandleDeleter<HWND, &DestroyWindow>>;

}// namespace OpenKneeboard
