// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <shims/winrt/base.h>

namespace OpenKneeboard {

class DebugPrivileges final {
 public:
  DebugPrivileges();
  ~DebugPrivileges();

  DebugPrivileges(const DebugPrivileges&) = delete;
  DebugPrivileges(DebugPrivileges&&) = delete;
  DebugPrivileges& operator=(const DebugPrivileges&) = delete;
  DebugPrivileges& operator=(DebugPrivileges&&) = delete;

 private:
  winrt::handle mToken;
  LUID mLuid;
};
}// namespace OpenKneeboard