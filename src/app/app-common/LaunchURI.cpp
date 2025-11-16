// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#include <OpenKneeboard/LaunchURI.hpp>

#include <shims/winrt/base.h>

#include <shellapi.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <unordered_map>

static std::unordered_map<std::wstring, std::function<void(std::string_view)>>
  gHandlers;

namespace OpenKneeboard {

void RegisterURIHandler(
  std::string_view schemeName,
  std::function<void(std::string_view)> handler) {
  gHandlers.insert_or_assign(
    std::wstring(winrt::to_hstring(schemeName)), handler);
}

task<void> LaunchURI(std::string_view uriStr) {
  auto uri = winrt::Windows::Foundation::Uri(winrt::to_hstring(uriStr));
  const std::wstring scheme(uri.SchemeName());
  if (gHandlers.contains(scheme)) {
    gHandlers.at(scheme)(uriStr);
    co_return;
  }

  if (scheme == L"https" || scheme == L"http") {
    co_await winrt::Windows::System::Launcher::LaunchUriAsync(uri);
    co_return;
  }

  std::wstring uriWstr(winrt::to_hstring(uriStr));
  ShellExecuteW(NULL, L"open", uriWstr.c_str(), nullptr, nullptr, SW_NORMAL);
}

}// namespace OpenKneeboard
