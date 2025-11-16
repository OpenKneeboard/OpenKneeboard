// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/TabletInfo.hpp>
#include <OpenKneeboard/TabletState.hpp>

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string>

namespace OpenKneeboard {

class WintabTablet final {
 public:
  enum class Priority {
    AlwaysActive,
    ForegroundOnly,
  };

  WintabTablet() = delete;
  WintabTablet(HWND window, Priority priority);
  ~WintabTablet();

  bool IsValid() const;
  Priority GetPriority() const;
  void SetPriority(Priority);

  bool CanProcessMessage(UINT message) const;
  bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);

  TabletState GetState() const;
  TabletInfo GetDeviceInfo() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
