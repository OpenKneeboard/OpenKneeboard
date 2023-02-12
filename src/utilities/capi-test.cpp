/*
 * OpenKneeboard C API Header
 *
 * Copyright (C) 2022-2023 Fred Emmott <fred@fredemmott.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * ----------------------------------------------------------------
 * ----- THE ABOVE LICENSE ONLY APPLIES TO THIS SPECIFIC FILE -----
 * ----------------------------------------------------------------
 *
 * The majority of OpenKneeboard is under a different license; check specific
 * files for more information.
 */

// clang-format off
#include <Windows.h>
#include <ShlObj_core.h>
#include <libloaderapi.h>
#include <winrt/base.h>
// clang-format on

#include <OpenKneeboard_CAPI.h>

#include <filesystem>
#include <iostream>

static std::filesystem::path GetOpenKneeboardDLLPath() {
  const auto fromEnv = _wgetenv(L"OPENKNEEBOARD_CAPI_DLL");
  if (fromEnv) {
    return fromEnv;
  }

  wchar_t* programFiles = nullptr;
  winrt::check_hresult(
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &programFiles));

  return std::filesystem::path(programFiles) / L"OpenKneeboard" / L"bin"
    / L"OpenKneeboard_CAPI.dll";
}

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cout << "Usage: capi-test MESSAGE_NAME [MESSAGE_VALUE]" << std::endl;
    return 1;
  }

  ///////////////////////////
  ///// 1. Find the DLL /////
  ///////////////////////////

  const auto dllPath = GetOpenKneeboardDLLPath();

  if (!std::filesystem::exists(dllPath)) {
    std::wcout << L"DLL '" << dllPath.wstring()
               << L"' does not exist. Install OpenKneeboard, or set the "
                  L"OPENKNEEBOARD_CAPI_DLL environment variable."
               << std::endl;
    return 2;
  }

  ///////////////////////////
  ///// 2. Load the DLL /////
  ///////////////////////////

  const auto dll = LoadLibraryW(dllPath.wstring().c_str());
  if (!dll) {
    std::cout << "Failed to load DLL: " << GetLastError() << std::endl;
    return 3;
  }

  ////////////////////////////////
  ///// 3. Find the function /////
  ////////////////////////////////

  const auto pfn_OpenKneeboard_send_utf8
    = reinterpret_cast<decltype(&OpenKneeboard_send_utf8)>(
      GetProcAddress(dll, "OpenKneeboard_send_utf8"));
  if (!pfn_OpenKneeboard_send_utf8) {
    std::cout << "Failed to find 'OpenKneeboard_send_utf8': " << GetLastError()
              << std::endl;
    return 4;
  }

  const std::string name {argv[1]};
  const std::string value {(argc == 3) ? argv[2] : ""};

  ////////////////////////////////
  ///// 4. Call the function /////
  ////////////////////////////////

  pfn_OpenKneeboard_send_utf8(
    name.data(), name.size(), value.data(), value.size());

  FreeLibrary(dll);
  return 0;
}
