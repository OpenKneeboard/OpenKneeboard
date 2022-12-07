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
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <Windows.h>
#include <shims/winrt/base.h>

#include <iostream>
#include <optional>
#include <shims/filesystem>

namespace OpenKneeboard {

static std::wstring GetDPrintResourceName(std::wstring_view key) {
  return std::format(L"{}.dprint.v1.{}", OpenKneeboard::ProjectNameW, key);
}

#define IPC_RESOURCE_NAME_FUNC(resource) \
  static std::wstring_view GetDPrint##resource##Name() { \
    static std::wstring sCache; \
    if (!sCache.empty()) { \
      return sCache; \
    } \
    sCache = GetDPrintResourceName(L#resource); \
    return sCache; \
  }
IPC_RESOURCE_NAME_FUNC(BufferReadyEvent)
IPC_RESOURCE_NAME_FUNC(DataReadyEvent)
IPC_RESOURCE_NAME_FUNC(Mapping)
IPC_RESOURCE_NAME_FUNC(Mutex)
#undef IPC_RESOURCE_NAME_FUNC

static DPrintSettings gSettings;
static std::wstring gPrefixW;

static DPrintMessageHeader gIPCMessageHeader;

static void WriteIPCMessage(std::wstring_view message) {
  {
    winrt::handle mutex {
      OpenMutexW(SYNCHRONIZE, false, GetDPrintMutexName().data())};
    if (!mutex) {
      return;
    }
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      return;
    }
  }

  winrt::handle bufferReadyEvent {
    OpenEventW(SYNCHRONIZE, false, GetDPrintBufferReadyEventName().data())};
  if (!bufferReadyEvent) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  winrt::handle dataReadyEvent {OpenEventW(
    SYNCHRONIZE | EVENT_MODIFY_STATE,
    false,
    GetDPrintDataReadyEventName().data())};
  if (!dataReadyEvent) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  if (WaitForSingleObject(bufferReadyEvent.get(), 100) == WAIT_TIMEOUT) {
    return;
  }

  winrt::handle mapping {CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    nullptr,
    PAGE_READWRITE,
    0,
    sizeof(DPrintMessage),
    GetDPrintMappingName().data())};
  if (!mapping) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  void* shm
    = MapViewOfFile(mapping.get(), FILE_MAP_WRITE, 0, 0, sizeof(DPrintMessage));
  if (!shm) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  scope_guard shmGuard([shm]() { UnmapViewOfFile(shm); });
  memset(shm, 0, sizeof(DPrintMessage));

  auto messageSHM = reinterpret_cast<DPrintMessage*>(shm);
  memcpy(messageSHM, &gIPCMessageHeader, sizeof(gIPCMessageHeader));
  memcpy(
    messageSHM->mMessage, message.data(), message.size() * sizeof(message[0]));
  SetEvent(dataReadyEvent.get());
}

static bool IsDebugStreamEnabledInRegistry() {
  static std::optional<bool> sCache;
  if (sCache) {
    return *sCache;
  }
  DWORD value = 0;
  for (auto hkey: {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    DWORD size = sizeof(value);
    if (
      RegGetValueW(
        hkey,
        L"SOFTWARE\\Fred Emmott\\OpenKneeboard",
        L"AlwaysWriteToDebugStream",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size)
      == ERROR_SUCCESS) {
      break;
    }
  }
  *sCache = (value != 0);
  return *sCache;
}

static inline bool IsDebugStreamEnabled() {
#ifdef DEBUG
  return true;
#endif
  return IsDebugStreamEnabledInRegistry() || IsDebuggerPresent();
}

static inline bool IsConsoleOutputEnabled() {
#ifdef DEBUG
  return true;
#endif
  return gSettings.consoleOutput == DPrintSettings::ConsoleOutputMode::ALWAYS;
}

void dprint(std::string_view message) {
  auto w = winrt::to_hstring(message);
  dprint(w);
}

void dprint(std::wstring_view message) {
  if (IsDebugStreamEnabled()) {
    auto output = std::format(L"[{}] {}\n", gPrefixW, message);
    OutputDebugStringW(output.c_str());
  }

  if (IsConsoleOutputEnabled()) {
    static bool sFirst = true;
    if (sFirst) {
      sFirst = false;
      if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
      }
      FILE* inStream {};
      FILE* outStream {};
      FILE* errStream {};
      freopen_s(&inStream, "CONIN$", "r", stdin);
      freopen_s(&outStream, "CONOUT$", "w", stdout);
      freopen_s(&errStream, "CONOUT$", "w", stderr);
    }

    std::wcout << message << std::endl;
  }

  WriteIPCMessage(message);
}

void DPrintSettings::Set(const DPrintSettings& settings) {
  gSettings = settings;
  gPrefixW = std::wstring(winrt::to_hstring(settings.prefix));

  memset(&gIPCMessageHeader, 0, sizeof(gIPCMessageHeader));
  gIPCMessageHeader.mProcessID = GetCurrentProcessId(),
  memcpy(
    gIPCMessageHeader.mPrefix,
    gPrefixW.c_str(),
    gPrefixW.size() * sizeof(wchar_t));
  GetModuleFileNameW(
    NULL,
    gIPCMessageHeader.mExecutable,
    sizeof(gIPCMessageHeader.mExecutable) / sizeof(wchar_t));
}

DPrintReceiver::DPrintReceiver() {
  scope_guard cleanup([this]() {
    if (mUsable) {
      return;
    }
    mMutex = {};
    mMapping = {};
    mDataReadyEvent = {};
    mBufferReadyEvent = {};
  });

  mMutex = {CreateMutexW(nullptr, true, GetDPrintMutexName().data())};
  if (mMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    OPENKNEEBOARD_BREAK;
    return;
  }
  if (!mMutex) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  mMapping = {CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    nullptr,
    PAGE_READWRITE,
    0,
    sizeof(DPrintMessage),
    GetDPrintMappingName().data())};
  if (!mMapping) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  mBufferReadyEvent = {CreateEventW(
    nullptr, false, false, GetDPrintBufferReadyEventName().data())};
  if (!mBufferReadyEvent) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  mDataReadyEvent = {
    CreateEventW(nullptr, false, false, GetDPrintDataReadyEventName().data())};
  if (!mDataReadyEvent) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  mSHM = reinterpret_cast<DPrintMessage*>(MapViewOfFile(
    mMapping.get(),
    FILE_MAP_READ | FILE_MAP_WRITE,
    0,
    0,
    sizeof(DPrintMessage)));
  if (!mSHM) {
    OPENKNEEBOARD_BREAK;
    return;
  }

  mUsable = true;
}

DPrintReceiver::~DPrintReceiver() {
  if (mSHM) {
    UnmapViewOfFile(mSHM);
  }
}

bool DPrintReceiver::IsUsable() const {
  return mUsable;
}

void DPrintReceiver::Run(std::stop_token stopToken) {
  if (!this->IsUsable()) {
    return;
  }

  winrt::handle stopEvent {CreateEventW(nullptr, false, false, nullptr)};
  std::stop_callback stopCallback(
    stopToken, [&]() { SetEvent(stopEvent.get()); });

  HANDLE handles[] = {
    mDataReadyEvent.get(),
    stopEvent.get(),
  };

  while (true) {
    // Unset canary
    memset(mSHM, 0xcf, sizeof(DPrintMessage));
    SetEvent(mBufferReadyEvent.get());

    auto result = WaitForMultipleObjects(
      sizeof(handles) / sizeof(handles[0]),
      handles,
      /* all = */ false,
      /* ms = */ INFINITE);

    if (stopToken.stop_requested()) {
      ResetEvent(mBufferReadyEvent.get());
      return;
    }

    this->OnMessage(*mSHM);
  }
}

}// namespace OpenKneeboard
