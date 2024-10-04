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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#pragma once
#include "WebViewBasedSettingsPage.g.h"

#include <OpenKneeboard/json.hpp>

#include <winrt/Microsoft.Web.WebView2.Core.h>

#include <expected>
#include <memory>

namespace OpenKneeboard {
struct JSNativeData;
}// namespace OpenKneeboard

namespace winrt::OpenKneeboardApp::implementation {
struct WebViewBasedSettingsPage
  : WebViewBasedSettingsPageT<WebViewBasedSettingsPage> {
  WebViewBasedSettingsPage();
  ~WebViewBasedSettingsPage();

  OpenKneeboard::fire_and_forget OnLoaded();
  OpenKneeboard::fire_and_forget OnNavigatedTo(
    const winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs&);

  OpenKneeboard::fire_and_forget OnWebMessageReceived(
    winrt::Microsoft::Web::WebView2::Core::CoreWebView2,
    winrt::Microsoft::Web::WebView2::Core::
      CoreWebView2WebMessageReceivedEventArgs);

 private:
  task<void> LoadSettingsPage();
  bool mHaveLoaded = false;

  struct Page {
    std::shared_ptr<OpenKneeboard::JSNativeData> mNative;
    std::string mJSClassName;
    winrt::guid mInstanceID {OpenKneeboard::random_guid()};
  };

  std::optional<Page> mPage;

  using JSResponse = std::expected<nlohmann::json, std::string>;
  task<JSResponse> JSAPI_ShowFolderPicker(nlohmann::json);
  task<JSResponse> JSAPI_NativeObjectToJSON(nlohmann::json);
};
}// namespace winrt::OpenKneeboardApp::implementation

namespace winrt::OpenKneeboardApp::factory_implementation {
struct WebViewBasedSettingsPage : WebViewBasedSettingsPageT<
                                    WebViewBasedSettingsPage,
                                    implementation::WebViewBasedSettingsPage> {
};
}// namespace winrt::OpenKneeboardApp::factory_implementation
