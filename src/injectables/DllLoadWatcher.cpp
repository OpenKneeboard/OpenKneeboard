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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "DllLoadWatcher.h"

#include "detours-ext.h"

// clang-format off
#include <Windows.h>
#include <SubAuth.h>
// clang-format on

#include <OpenKneeboard/dprint.h>
#include <shims/winrt.h>

#include "detours-ext.h"

// There's a header for this, but only in the DDK
typedef const UNICODE_STRING* PCUNICODE_STRING;

// Documented at
// https://docs.microsoft.com/en-us/windows/win32/devnotes/ldrregisterdllnotification
// but there are no headers
enum LDR_DLL_LOADED_NOTIFICATION_REASON {
  LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
  LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2
};

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
  ULONG Flags;// Reserved.
  PCUNICODE_STRING FullDllName;// The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;// The base file name of the DLL module.
  PVOID DllBase;// A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;// The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
  ULONG Flags;// Reserved.
  PCUNICODE_STRING FullDllName;// The full path name of the DLL module.
  PCUNICODE_STRING BaseDllName;// The base file name of the DLL module.
  PVOID DllBase;// A pointer to the base address for the DLL in memory.
  ULONG SizeOfImage;// The size of the DLL image, in bytes.
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
  LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
  LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef const PLDR_DLL_NOTIFICATION_DATA PCLDR_DLL_NOTIFICATION_DATA;

VOID CALLBACK LdrDllNotification();

typedef VOID(CALLBACK* PLDR_DLL_NOTIFICATION_FUNCTION)(
  _In_ ULONG /* flags */,
  _In_ PCLDR_DLL_NOTIFICATION_DATA /* data */,
  _In_opt_ PVOID /* context */);

NTSTATUS NTAPI LdrRegisterDllNotification(
  _In_ ULONG Flags,
  _In_ PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
  _In_opt_ PVOID Context,
  _Out_ PVOID* Cookie);

NTSTATUS NTAPI LdrUnregisterDllNotification(_In_ PVOID Cookie);

namespace OpenKneeboard {

struct DllLoadWatcher::Impl {
  Callbacks mCallbacks;
  void* mCookie = nullptr;

  std::string mName;

  static void CALLBACK
  OnNotification(ULONG reason, PCLDR_DLL_NOTIFICATION_DATA data, void* context);

 private:
  static DWORD WINAPI NotificationThread(void* arg);
};

DllLoadWatcher::DllLoadWatcher(std::string_view name)
  : p(std::make_unique<Impl>()) {
  p->mName = std::string(name);
}

bool DllLoadWatcher::IsDllLoaded() const {
  return GetModuleHandleA(p->mName.c_str());
}

void DllLoadWatcher::InstallHook(const Callbacks& callbacks) {
  if (!callbacks.onDllLoaded) {
    throw std::logic_error("DLL load hook with no onDllLoaded callback?");
  }

  p->mCallbacks = callbacks;

  auto f = reinterpret_cast<decltype(&LdrRegisterDllNotification)>(
    DetourFindFunction("Ntdll.dll", "LdrRegisterDllNotification"));
  if (!f) {
    dprintf("Failed to find LdrRegisterDllNotification");
    return;
  }
  auto status = f(
    0, &Impl::OnNotification, reinterpret_cast<void*>(p.get()), &p->mCookie);
  if (status != STATUS_SUCCESS) {
    dprintf(
      "Failed to LdrRegisterDllNotification for {}: {}", p->mName, status);
    return;
  }
  dprintf("DllLoadWatcher++ {}", p->mName);
  if (callbacks.onHookInstalled) {
    callbacks.onHookInstalled();
  }
}

DllLoadWatcher::~DllLoadWatcher() {
  this->UninstallHook();
}

void DllLoadWatcher::UninstallHook() {
  if (!p->mCookie) {
    // We never installed the hook, or it's already uninstalled
    return;
  }
  auto f = reinterpret_cast<decltype(&LdrUnregisterDllNotification)>(
    DetourFindFunction("Ntdll.dll", "LdrUnregisterDllNotification"));
  if (!f) {
    dprintf("Failed to find LdrUnregisterDllNotification");
    return;
  }
  auto status = f(p->mCookie);
  if (status != STATUS_SUCCESS) {
    dprintf("Failed to unload watcher for {}: {}", p->mName, status);
    return;
  }
  dprintf("DllLoadWatcher-- {}", p->mName);
  p->mCookie = nullptr;
}

DWORD WINAPI DllLoadWatcher::Impl::NotificationThread(void* _arg) {
  auto p = reinterpret_cast<Impl*>(_arg);

  // While we know the desired library is in the process of being loaded,
  // we need to wait until it's completely loaded.
  //
  // As LoadLibraryA internally locks, we can just call it again, at the
  // cost of an extra incref+decref.
  FreeLibrary(LoadLibraryA(p->mName.c_str()));
  p->mCallbacks.onDllLoaded();
  return S_OK;
}

void CALLBACK DllLoadWatcher::Impl::OnNotification(
  ULONG reason,
  PCLDR_DLL_NOTIFICATION_DATA data,
  void* context) {
  auto p = reinterpret_cast<Impl*>(context);
  if (reason != LDR_DLL_NOTIFICATION_REASON_LOADED) {
    return;
  }

  auto wname = data->Loaded.BaseDllName;
  auto name = winrt::to_string(
    std::wstring_view(wname->Buffer, wname->Length / sizeof(wname->Buffer[0])));

  if (name != p->mName) {
    return;
  }

  CreateThread(nullptr, 0, &NotificationThread, p, 0, nullptr);
}

}// namespace OpenKneeboard
