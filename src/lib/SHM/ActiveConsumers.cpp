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
#include <OpenKneeboard/SHM/ActiveConsumers.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/version.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <format>
#include <string>

namespace OpenKneeboard::SHM {

class ActiveConsumers::Impl {
 public:
  Impl() {
    SetLastError(0);
    mFileHandle = winrt::file_handle {CreateFileMappingW(
      INVALID_HANDLE_VALUE,
      NULL,
      PAGE_READWRITE,
      0,
      static_cast<DWORD>(sizeof(ActiveConsumers)),
      GetSHMPath().c_str())};
    if (!mFileHandle) {
      return;
    }
    const auto created = (GetLastError() != ERROR_ALREADY_EXISTS);
    mView = reinterpret_cast<ActiveConsumers*>(MapViewOfFile(
      mFileHandle.get(), FILE_MAP_WRITE, 0, 0, sizeof(ActiveConsumers)));
    if (created) {
      *mView = {};
    }
  }

  ~Impl() {
    if (mView) {
      auto view = mView;
      mView = nullptr;
      UnmapViewOfFile(view);
    }
  }

  static ActiveConsumers* Get() {
    static Impl sImpl;
    return sImpl.mView;
  }

 private:
  winrt::file_handle mFileHandle;
  ActiveConsumers* mView {nullptr};
  static std::wstring GetSHMPath() {
    static std::wstring sCache;
    if (!sCache.empty()) [[likely]] {
      return sCache;
    }
    sCache = std::format(
      L"{}/{}.{}.{}.{}/ActiveConsumers-s{:x}",
      ProjectNameW,
      Version::Major,
      Version::Minor,
      Version::Patch,
      Version::Build,
      sizeof(ActiveConsumers));
    return sCache;
  }
};

void ActiveConsumers::Clear() {
  auto p = Impl::Get();
  if (p) {
    *p = {};
  }
}

ActiveConsumers ActiveConsumers::Get() {
  auto p = Impl::Get();
  if (p) {
    return *p;
  }
  return {};
}

void ActiveConsumers::Set(ConsumerKind consumer) {
  auto p = Impl::Get();
  if (!p) {
    return;
  }
  const auto now = Clock::now();
  switch (consumer) {
    case ConsumerKind::SteamVR:
      p->mSteamVR = now;
      break;
    case ConsumerKind::OpenXR:
      p->mOpenXR = now;
      break;
    case ConsumerKind::OculusD3D11:
      p->mOculusD3D11 = now;
      break;
    case ConsumerKind::OculusD3D12:
      p->mOculusD3D12 = now;
      break;
    case ConsumerKind::NonVRD3D11:
      p->mNonVRD3D11 = now;
      break;
    case ConsumerKind::Viewer:
      p->mViewer = now;
      break;
  }
}

}// namespace OpenKneeboard::SHM