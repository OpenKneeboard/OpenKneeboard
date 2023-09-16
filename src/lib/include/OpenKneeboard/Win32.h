/*
 * OpenKneeboard
 *
 * Copyright (C) 2023 Fred Emmott <fred@fredemmott.com>
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
#include <shims/winrt/base.h>

namespace OpenKneeboard::Win32 {

// Wrappers around Win32 functions
//
// This currently contains wrappers around functions that return `HANDLE`, where
// the wrappers either return a `winrt::handle` for functions that may return
// `NULL`, or a `winrt::file_handle` for functions that may return
// `INVALID_HANDLE_VALUE`.

namespace detail {

template <class THandle, class TFun, TFun fun>
struct HandleWrapper;

template <class THandle, class... TArgs, HANDLE(__stdcall* fun)(TArgs...)>
struct HandleWrapper<THandle, HANDLE(__stdcall*)(TArgs...), fun> {
  static constexpr THandle wrap(TArgs&&... args) {
    return THandle {fun(std::forward<TArgs>(args)...)};
  }
};

}// namespace detail

///// May return NULL /////

#define IT(FUN) \
  constexpr auto FUN \
    = detail::HandleWrapper<winrt::handle, decltype(&::FUN), &::FUN>::wrap;
IT(CreateEventW);
IT(CreateFileMappingW);
IT(CreateMutexW);
IT(CreateWaitableTimerW);
#undef IT

///// May return INVALID_HANDLE_VALUE /////

#define IT(FUN) \
  constexpr auto FUN = detail:: \
    HandleWrapper<winrt::file_handle, decltype(&::FUN), ::FUN>::wrap;
IT(CreateFileW);
IT(CreateMailslotW);
#undef IT

}// namespace OpenKneeboard::Win32