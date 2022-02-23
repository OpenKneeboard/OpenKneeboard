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
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/dprint.h>
#include <Psapi.h>
#include <shims/wx.h>

#include <filesystem>

#include "okMainWindow.h"

static bool ActivateExistingInstance() {
  // If already running, make the other instance active.
  //
  // There's more common ways to do this, but given we already have the SHM
  // and the SHM has a handy HWND, might as well use that.
  OpenKneeboard::SHM::Reader shm;
  if (!shm) {
    return false;
  }
  auto snapshot = shm.MaybeGet();
  if (!snapshot) {
    return false;
  }
  const auto hwnd = snapshot.GetConfig().feederWindow;
  if (!hwnd) {
    return false;
  }
  DWORD processID = 0;
  GetWindowThreadProcessId(hwnd, &processID);
  if (!processID) {
    return false;
  }
  winrt::handle processHandle {
    OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processID)};
  if (!processHandle) {
    return false;
  }
  wchar_t processName[MAX_PATH];
  auto processNameLen
    = GetProcessImageFileNameW(processHandle.get(), processName, MAX_PATH);
  const auto processStem
    = std::filesystem::path(std::wstring_view(processName, processNameLen))
        .stem();
  processNameLen = GetModuleFileNameW(NULL, processName, MAX_PATH);
  const auto thisProcessStem
    = std::filesystem::path(std::wstring_view(processName, processNameLen))
        .stem();
  if (processStem != thisProcessStem) {
    return false;
  }
  return ShowWindow(hwnd, SW_SHOWNORMAL) && SetForegroundWindow(hwnd);
}

class OpenKneeboardApp final : public wxApp {
 public:
  virtual bool OnInit() override {
    if (ActivateExistingInstance()) {
      return false;
    }
    OpenKneeboard::DPrintSettings::Set({
      .prefix = "OpenKneeboard",
    });

    wxInitAllImageHandlers();
    (new okMainWindow())->Show();
    return true;
  }
};

wxIMPLEMENT_APP(OpenKneeboardApp);
