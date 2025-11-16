// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/DebugPrivileges.hpp>

#include <OpenKneeboard/dprint.hpp>

namespace OpenKneeboard {
static bool sHaveInstance = false;

DebugPrivileges::DebugPrivileges() {
  if (sHaveInstance) {
    fatal("Can't nest DebugPrivileges");
  }
  sHaveInstance = true;
  if (!OpenProcessToken(
        GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
        mToken.put())) {
    dprint.Error(
      "Failed to open own process token : {:#018x}",
      HRESULT_FROM_WIN32(GetLastError()));
    return;
  }

  LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &mLuid);

  TOKEN_PRIVILEGES privileges;
  privileges.PrivilegeCount = 1;
  privileges.Privileges[0].Luid = mLuid;
  privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  if (!AdjustTokenPrivileges(
        mToken.get(),
        false,
        &privileges,
        sizeof(privileges),
        nullptr,
        nullptr)) {
    const auto code = GetLastError();
    const auto message
      = std::system_category().default_error_condition(code).message();
    dprint.Warning(
      "Failed to acquire debug privileges: {:#x} ({})",
      std::bit_cast<uint32_t>(code),
      message);
  }
  dprint("Acquired debug privileges");
}

DebugPrivileges::~DebugPrivileges() {
  sHaveInstance = false;
  if (!mToken) {
    return;
  }
  TOKEN_PRIVILEGES privileges;
  privileges.PrivilegeCount = 1;
  privileges.Privileges[0].Luid = mLuid;
  privileges.Privileges[0].Attributes = SE_PRIVILEGE_REMOVED;
  AdjustTokenPrivileges(
    mToken.get(), false, &privileges, sizeof(privileges), nullptr, nullptr);
  dprint("Released debug privileges");
}

}// namespace OpenKneeboard