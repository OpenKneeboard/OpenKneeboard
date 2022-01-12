#include "DllLoadWatcher.h"

// clang-format off
#include <Windows.h>
#include <SubAuth.h>
// clang-format on

#include <OpenKneeboard/dprint.h>
#include <winrt/base.h>

#include "detours-ext.h"

// There's a header for this, but only in the DDK
typedef const UNICODE_STRING* PCUNICODE_STRING;

// Documented at https://docs.microsoft.com/en-us/windows/win32/devnotes/ldrregisterdllnotification
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
  DllLoadWatcher* mWatcher = nullptr;
  void* mCookie = nullptr;

  std::string mName;

  static void CALLBACK
  OnNotification(ULONG reason, PCLDR_DLL_NOTIFICATION_DATA data, void* context);

 private:
  static DWORD WINAPI NotificationThread(void* arg);
};

DllLoadWatcher::DllLoadWatcher(const std::string& name)
  : p(std::make_unique<Impl>()) {
  p->mWatcher = this;
  p->mName = name;
  auto f = reinterpret_cast<decltype(&LdrRegisterDllNotification)>(
    DetourFindFunction("Ntdll.dll", "LdrRegisterDllNotification"));
  if (!f) {
    dprintf("Failed to find LdrRegisterDllNotification");
    return;
  }
  f(0, &Impl::OnNotification, reinterpret_cast<void*>(p.get()), &p->mCookie);
}

DllLoadWatcher::~DllLoadWatcher() {
  if (!p->mCookie) {
    return;
  }
  auto f = reinterpret_cast<decltype(&LdrUnregisterDllNotification)>(
    DetourFindFunction("Ntdll.dll", "LdrUnregisterDllNotification"));
  if (!f) {
    dprintf("Failed to find LdrUnregisterDllNotification");
    return;
  }
  f(p->mCookie);
}

DWORD WINAPI DllLoadWatcher::Impl::NotificationThread(void* _arg) {
  auto p = reinterpret_cast<Impl*>(_arg);

  // While we know the desired library is in the process of being loaded,
  // we need to wait until it's completely loaded.
  //
  // As LoadLibraryA internally locks, we can just call it again, at the
  // cost of an extra incref+decref.
  FreeLibrary(LoadLibraryA(p->mName.c_str()));
  p->mWatcher->OnDllLoad(p->mName);
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
