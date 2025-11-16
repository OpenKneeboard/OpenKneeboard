// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/Elevation.hpp>
#include <OpenKneeboard/SHM/ActiveConsumers.hpp>

#include <OpenKneeboard/config.hpp>
#include <OpenKneeboard/version.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <algorithm>
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
    if (!mView) {
      mFileHandle = {};
      return;
    }
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

  Impl(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl& operator=(Impl&&) = delete;

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
      ProjectReverseDomainW,
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

  static bool sHaveCheckedElevation {false};
  if (!sHaveCheckedElevation) {
    if (IsElevated()) {
      p->mElevatedConsumerProcessID = GetCurrentProcessId();
    }
    sHaveCheckedElevation = true;
  }

  const auto now = Clock::now();
  switch (consumer) {
    case ConsumerKind::OpenVR:
      p->mOpenVR = now;
      break;
    case ConsumerKind::OpenXR:
      p->mOpenXR = now;
      break;
    case ConsumerKind::OculusD3D11:
      p->mOculusD3D11 = now;
      break;
    case ConsumerKind::NonVRD3D11:
      p->mNonVRD3D11 = now;
      break;
    case ConsumerKind::Viewer:
      p->mViewer = now;
      break;
  }
}

void ActiveConsumers::SetNonVRPixelSize(PixelSize px) {
  auto p = Impl::Get();
  if (!p) {
    return;
  }

  p->mNonVRPixelSize = px;
}

void ActiveConsumers::SetActiveInGameViewID(uint64_t id) {
  auto p = Impl::Get();
  if (!p) {
    return;
  }

  p->mActiveInGameViewID = id;
}

ActiveConsumers::T ActiveConsumers::Any() const {
  const auto ret
    = std::ranges::max({mOpenVR, mOpenXR, mOculusD3D11, mNonVRD3D11});
  if (ret != T {}) {
    return ret;
  }
  return mViewer;
}

ActiveConsumers::T ActiveConsumers::AnyVR() const {
  return std::ranges::max({mOpenVR, mOpenXR, mOculusD3D11});
}

ActiveConsumers::T ActiveConsumers::VRExceptSteam() const {
  return std::ranges::max({mOpenXR, mOculusD3D11});
}

ActiveConsumers::T ActiveConsumers::NotVROrViewer() const {
  return mNonVRD3D11;
}

ActiveConsumers::T ActiveConsumers::NotVR() const {
  const auto real = NotVROrViewer();
  if (real != T {}) {
    return real;
  }
  return mViewer;
}

}// namespace OpenKneeboard::SHM