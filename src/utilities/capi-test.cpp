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

#include <Windows.h>
#include <ShlObj_core.h>
#include <libloaderapi.h>

#include <winrt/base.h>

#include <filesystem>
#include <iostream>
#include <optional>

#include <OpenKneeboard_CAPI.h>

static std::filesystem::path GetOpenKneeboardDLLPath();

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

  // not using bit_cast as we intentionally target an earlier C++ version with
  // this test executable
  const auto pfn_OpenKneeboard_send_utf8 =
    reinterpret_cast<decltype(&OpenKneeboard_send_utf8)>(
      reinterpret_cast<void*>(GetProcAddress(dll, "OpenKneeboard_send_utf8")));
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

static std::optional<std::filesystem::path>
GetOpenKneeboardDLLPathFromEnvironment() {
  // When C++23 is widely available, a combination of `std::unique_ptr` and
  // `std::out_ptr` would be better than calling `free()`; there's
  // third-party implementations of `out_ptr` for earlier versions of C++,
  // but I'm avoiding additional dependencies for this example.

  wchar_t* fromEnv {nullptr};
  size_t fromEnvCharCount {};
  if (
    _wdupenv_s(&fromEnv, &fromEnvCharCount, L"OPENKNEEBOARD_CAPI_DLL") != 0
    || !fromEnv) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    free(fromEnv);
    return {};
  }

  const std::filesystem::path ret {
    std::wstring_view {fromEnv, fromEnvCharCount - 1}};
  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
  free(fromEnv);
  return ret;
}

// Requires OpenKneeboard v1.8.4 or above
static std::optional<std::filesystem::path>
GetOpenKneeboardDLLPathFromRegistry() {
  DWORD regType {REG_SZ};
  wchar_t regValue[MAX_PATH];
  DWORD regValueByteCount {std::size(regValue) * sizeof(wchar_t)};
  if (
    RegGetValueW(
      HKEY_CURRENT_USER,
      L"Software\\Fred Emmott\\OpenKneeboard",
      L"InstallationBinPath",
      // Always use the 64-bit registry, even if built as 32-bits
      RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY | RRF_ZEROONFAILURE,
      &regType,
      regValue,
      &regValueByteCount)
      != ERROR_SUCCESS
    || regType != REG_SZ || regValueByteCount <= 0) {
    return {};
  }

  return std::filesystem::path {std::wstring_view {
           regValue, (regValueByteCount / sizeof(wchar_t)) - 1}}
  / OPENKNEEBOARD_CAPI_DLL_NAME_W;
}

static std::filesystem::path GetOpenKneeboardDLLPath() {
  if (const auto ret = GetOpenKneeboardDLLPathFromEnvironment()) {
    return *ret;
  }

  if (const auto ret = GetOpenKneeboardDLLPathFromRegistry()) {
    return *ret;
  }

  // Fall back to Program Files for v1.8.3 and below; this should be removed
  // once v1.8.4+ are widespread

  wchar_t* programFiles = nullptr;
  try {
    winrt::check_hresult(SHGetKnownFolderPath(
      FOLDERID_ProgramFilesX64, 0, nullptr, &programFiles));
  } catch (...) {
    CoTaskMemFree(programFiles);
    throw;
  }

  const auto ret = std::filesystem::path(programFiles) / L"OpenKneeboard"
    / L"bin" / OPENKNEEBOARD_CAPI_DLL_NAME_W;

  // Another opportunity for std::unique_ptr + std::out_ptr when using C++23
  CoTaskMemFree(programFiles);
  return ret;
}
