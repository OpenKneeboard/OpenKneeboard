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
#include <AccCtrl.h>
#include <AclAPI.h>
#include <OpenKneeboard/FilesDiffer.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/RuntimeFiles.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <shims/winrt/base.h>

#include <vector>

namespace OpenKneeboard::RuntimeFiles {

static SID WorldSID() {
  SID sid {};
  DWORD sidSize = sizeof(sid);
  winrt::check_bool(CreateWellKnownSid(WinWorldSid, NULL, &sid, &sidSize));
  return sid;
}

using unique_SID_ptr = std::unique_ptr<SID, std::function<void(SID*)>>;

template <class... SubAuthorities>
static unique_SID_ptr SIDFromSubAuthorities(
  SID_IDENTIFIER_AUTHORITY authority,
  SubAuthorities... subs) {
  SID* sid {nullptr};

  constexpr auto subCount = sizeof...(subs);
  static_assert(subCount <= 8);
  std::array<DWORD, subCount> subArr {static_cast<DWORD>(subs)...};

  winrt::check_bool(AllocateAndInitializeSid(
    &authority,
    subCount,
    subCount > 0 ? subArr[0] : 0,
    subCount > 1 ? subArr[1] : 0,
    subCount > 2 ? subArr[2] : 0,
    subCount > 3 ? subArr[3] : 0,
    subCount > 4 ? subArr[4] : 0,
    subCount > 5 ? subArr[5] : 0,
    subCount > 6 ? subArr[6] : 0,
    subCount > 7 ? subArr[7] : 0,
    reinterpret_cast<PSID*>(&sid)));

  return {sid, [](SID* sid) { FreeSid(sid); }};
}

static unique_SID_ptr AllPackagesSID() {
  return SIDFromSubAuthorities(
    SECURITY_APP_PACKAGE_AUTHORITY,
    SECURITY_APP_PACKAGE_BASE_RID,
    SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE);
}

static unique_SID_ptr AllRestrictedPackagesSID() {
  return SIDFromSubAuthorities(
    SECURITY_APP_PACKAGE_AUTHORITY,
    SECURITY_APP_PACKAGE_BASE_RID,
    SECURITY_BUILTIN_PACKAGE_ANY_RESTRICTED_PACKAGE);
}

/** Make the specified path readable within a Windows App container/sandbox.
 *
 * This is pretty much entirely to make "OpenXR Tools for Windows Mixed Reality"
 * happy; as it's sandboxed, it's not representative of real-world games, but
 * might as well make it so `xrCreateInstance` doesn't fail there if
 * OpenKneeboard's OpenXR support is installed for all users.
 *
 * Real games don't need this as:
 * - they all (?) run unsandboxed, so the default 'Users' grant for ProgramData
 *   works
 * - if there is a sandboxed game, it probably can't access the SHM or shared
 *   GPU textures anyway
 */
static void MakeReadableFromSandboxedApps(
  const std::filesystem::path& path) noexcept {
  PACL oldACL {nullptr};
  PSECURITY_DESCRIPTOR securityDescriptor {nullptr};

  winrt::check_win32(GetNamedSecurityInfoW(
    path.wstring().c_str(),
    SE_FILE_OBJECT,
    DACL_SECURITY_INFORMATION,
    nullptr,
    nullptr,
    &oldACL,
    nullptr,
    &securityDescriptor));
  const scope_guard freeSecurityDescriptor(
    [&]() { LocalFree(securityDescriptor); });

  EXPLICIT_ACCESS_W grants[3] {};
  auto worldSid = WorldSID();
  grants[0] = EXPLICIT_ACCESS_W {
    .grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE | READ_CONTROL,
    .grfAccessMode = GRANT_ACCESS,
    .grfInheritance = OBJECT_INHERIT_ACE,
    .Trustee = TRUSTEE_W {
      .TrusteeForm = TRUSTEE_IS_SID,
      .TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP,
      .ptstrName = reinterpret_cast<LPWCH>(&worldSid),
    },
  };
  grants[1] = grants[0];
  grants[2] = grants[0];
  auto allPackagesSid = AllPackagesSID();
  auto allRestrictedPackagesSid = AllRestrictedPackagesSID();
  BuildTrusteeWithSidW(&grants[1].Trustee, allPackagesSid.get());
  BuildTrusteeWithSidW(&grants[2].Trustee, allRestrictedPackagesSid.get());

  PACL newACL {nullptr};
  winrt::check_win32(SetEntriesInAclW(3, grants, oldACL, &newACL));
  const scope_guard freeACL([&]() { LocalFree(newACL); });

  winrt::check_win32(SetNamedSecurityInfoW(
    const_cast<LPWSTR>(path.wstring().c_str()),
    SE_FILE_OBJECT,
    DACL_SECURITY_INFORMATION,
    nullptr,
    nullptr,
    newACL,
    nullptr));
}

void Install() {
  const auto source = Filesystem::GetRuntimeDirectory();
  const auto destination = RuntimeFiles::GetInstallationDirectory();
  std::filesystem::create_directories(destination);
  MakeReadableFromSandboxedApps(destination);

#define IT(file) \
  if (FilesDiffer(source / file, destination / file)) { \
    std::filesystem::copy( \
      source / file, \
      destination / file, \
      std::filesystem::copy_options::overwrite_existing); \
  }

  OPENKNEEBOARD_PUBLIC_RUNTIME_FILES
#undef IT
}

}// namespace OpenKneeboard::RuntimeFiles
