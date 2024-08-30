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
