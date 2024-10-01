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
// clang-format off
#include "pch.h"
#include "WebViewBasedSettingsPage.xaml.h"
#include "WebViewBasedSettingsPage.g.cpp"
// clang-format on

#include "FilePicker.h"

#include <OpenKneeboard/DeveloperToolsSettingsPage.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/Win32.hpp>

#include <OpenKneeboard/format/filesystem.hpp>

#include <winrt/Microsoft.Web.WebView2.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <magic_enum.hpp>

using namespace ::OpenKneeboard;

using namespace OpenKneeboard;

namespace winrt::OpenKneeboardApp::implementation {

static const auto VirtualHost = L"openkneeboard-app.localhost";

WebViewBasedSettingsPage::WebViewBasedSettingsPage() {
  InitializeComponent();
  this->Loaded(
    &WebViewBasedSettingsPage::OnLoaded | bind_front(this)
    | drop_winrt_event_args());
}

WebViewBasedSettingsPage::~WebViewBasedSettingsPage() {
}

OpenKneeboard::fire_and_forget WebViewBasedSettingsPage::OnNavigatedTo(
  const winrt::Microsoft::UI::Xaml::Navigation::NavigationEventArgs& e) {
  const auto pageName
    = winrt::to_string(winrt::unbox_value<winrt::hstring>(e.Parameter()));

  if (pageName == "DeveloperTools") {
    mPage = Page {
      .mNative
      = std::make_shared<DeveloperToolsSettingsPage>(gKneeboard.lock().get()),
    };
  } else {
    dprint.Error("Invalid WebView settings page name: {}", pageName);
    co_return;
  }

  mPage->mJSClassName = std::format("{}SettingsPage", pageName);

  if (mHaveLoaded) {
    co_await this->LoadSettingsPage();
  }
  co_return;
}

OpenKneeboard::fire_and_forget WebViewBasedSettingsPage::OnLoaded() {
  using namespace winrt::Microsoft::Web::WebView2::Core;

  CoreWebView2EnvironmentOptions environmentOptions {};
  environmentOptions.AdditionalBrowserArguments(
    "--disable-features=msSmartScreenProtection"_hs);

  const auto persistRoot
    = Filesystem::GetLocalAppDataDirectory() / "App-WebView2";

  auto environment = co_await CoreWebView2Environment::CreateWithOptionsAsync(
    {},
    (Filesystem::GetLocalAppDataDirectory() / "App-WebView2").wstring(),
    environmentOptions);

  co_await WebViewControl().EnsureCoreWebView2Async(environment);

  auto webView = WebViewControl().CoreWebView2();
  webView.WebMessageReceived(
    &WebViewBasedSettingsPage::OnWebMessageReceived | bind_refs_front(this));

  mHaveLoaded = true;
  if (mPage) {
    co_await this->LoadSettingsPage();
  }
}

template <class T>
static constexpr auto MetaInvokeTyped(auto& page, auto fn) {
  return std::invoke(fn, static_cast<T&>(*page->mNative.get()), JSClass<T> {});
}

static constexpr auto MetaInvoke(auto& page, auto fn) {
  if (page->mJSClassName == "DeveloperToolsSettingsPage") {
    return MetaInvokeTyped<DeveloperToolsSettingsPage>(page, fn);
  }
  std::unreachable();
}

static nlohmann::json GetSystemUIColors() {
  nlohmann::json ret;

  using namespace winrt::Windows::UI::ViewManagement;

  UISettings uiSettings {};

  for (auto&& [enumValue, name]: magic_enum::enum_entries<UIColorType>()) {
    if (enumValue == UIColorType::Complement) {
      // Deprecated, documented as throwing
      continue;
    }
    const auto [a, r, g, b] = uiSettings.GetColorValue(enumValue);
    ret.emplace(
      name, std::format("rgba({}, {}, {}, {:0.2f})", r, g, b, a / 255.0));
  }

  return ret;
}

static nlohmann::json GetSystemUIElementColors() {
  nlohmann::json ret;

  using namespace winrt::Windows::UI::ViewManagement;

  UISettings uiSettings {};

  for (auto&& [enumValue, name]: magic_enum::enum_entries<UIElementType>()) {
    const auto [a, r, g, b] = uiSettings.UIElementColor(enumValue);
    ret.emplace(
      name, std::format("rgba({}, {}, {}, {:0.2f})", r, g, b, a / 255.0));
  }

  return ret;
}

task<void> WebViewBasedSettingsPage::LoadSettingsPage() {
  using namespace winrt::Microsoft::Web::WebView2::Core;

  std::vector<std::string> warnings;

  std::filesystem::path sourcePath;
  {
    const auto app = gKneeboard.lock()->GetAppSettings();
    sourcePath = app.GetAppWebViewSourcePath();

    const auto buildArtifactSuffix
      = std::filesystem::path {"dist"} / "SettingsPage.js";
    const auto buildArtifact = sourcePath / buildArtifactSuffix;
    if (!(std::filesystem::exists(buildArtifact))) {
      warnings.push_back(std::format(
        _("Configured source `{}` does not exist, falling "
          "back to built-in content"),
        buildArtifact));
      sourcePath = app.GetDefaultAppWebViewSourcePath();
      OPENKNEEBOARD_ASSERT(
        std::filesystem::exists(sourcePath / buildArtifactSuffix));
    } else if (!app.mAppWebViewSourcePath.empty()) {
      warnings.push_back(std::format(
        _("Using webview content from {}"), app.mAppWebViewSourcePath));
    }
  }

  for (const auto& warning: warnings) {
    dprint.Warning("WebView settings page: {}", warning);
  }
  const nlohmann::json initData {
    {"class", mPage->mJSClassName},
    {"instanceID", std::format("{:nobraces}", mPage->mInstanceID)},
    {
      "systemTheme",
      {
        {"uiColors", GetSystemUIColors()},
        {"uiElementColors", GetSystemUIElementColors()},
      },
    },
    {"warnings", warnings},
  };

  auto webView = WebViewControl().CoreWebView2();

  co_await webView.AddScriptToExecuteOnDocumentCreatedAsync(
    winrt::to_hstring(std::format("window.InitData = {};", initData.dump(2))));
  webView.SetVirtualHostNameToFolderMapping(
    VirtualHost,
    sourcePath.wstring(),
    CoreWebView2HostResourceAccessKind::Allow);
  webView.Navigate(std::format(L"https://{}/SettingsPage.html", VirtualHost));

  co_return;
}

OpenKneeboard::fire_and_forget WebViewBasedSettingsPage::OnWebMessageReceived(
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2,
  winrt::Microsoft::Web::WebView2::Core::CoreWebView2WebMessageReceivedEventArgs
    args) {
  auto self = this->get_strong();
  if (!mPage) {
    co_return;
  }

  const auto json = to_string(args.WebMessageAsJson());
  const auto parsed = nlohmann::json::parse(json);

  if (!parsed.contains("message")) {
    co_return;
  }
  const auto messageKind = parsed.at("message").get<std::string>();

  if (messageKind == "NativePropertyChanged") {
    if (
      mPage->mInstanceID
      != winrt::guid {parsed.at("instanceID").get<std::string_view>()}) {
      co_return;
    }
    const std::string_view name = parsed.at("propertyName");
    const auto& value = parsed.at("propertyValue");
    MetaInvoke(mPage, [&](auto& native, auto js) {
      js.SetPropertyByName(native, name, value);
    });
    co_return;
  }

  if (messageKind == "InvokeNativeMethod") {
    if (
      mPage->mInstanceID
      != winrt::guid {parsed.at("instanceID").get<std::string_view>()}) {
      co_return;
    }
    const std::string_view name = parsed.at("methodName");
    MetaInvoke(mPage, [&](auto& native, auto js) {
      js.InvokeMethodByName(native, name);
    });
    co_return;
  }

  const auto jsapi_handler = [&](const auto& impl) -> task<void> {
    auto result = co_await std::invoke(impl, this, parsed.at("params"));
    nlohmann::json response {
      {"message", "ResolvePromise"},
      {"promiseID", parsed.at("promiseID")},
      {"success", result.has_value()},
    };
    if (result.has_value()) {
      response.emplace("result", *result);
    } else {
      response.emplace("error", result.error());
    }
    WebViewControl().CoreWebView2().PostWebMessageAsJson(
      winrt::to_hstring(response.dump()));
  };

#define IT(x) \
  if (messageKind == #x) { \
    co_await jsapi_handler(&WebViewBasedSettingsPage::JSAPI_##x); \
    co_return; \
  }
  IT(ShowFolderPicker)
  IT(NativeObjectToJSON)
#undef IT

  co_return;
}

task<WebViewBasedSettingsPage::JSResponse>
WebViewBasedSettingsPage::JSAPI_ShowFolderPicker(const nlohmann::json params) {
  auto self = this->get_strong();
  co_await wil::resume_foreground(DispatcherQueue());

  FilePicker picker(gMainWindow);
  if (params.contains("title")) {
    picker.SetTitle(
      *FredEmmott::winapi::utf8_traits::to_wide(params.at("title")));
  }
  if (params.contains("persistenceGuid")) {
    picker.SettingsIdentifier(
      winrt::guid(params.at("persistenceGuid").get<std::string>()));
  }
  if (params.contains("startLocationGuid")) {
    picker.SuggestedStartLocation(
      winrt::guid(params.at("startLocationGuid").get<std::string>()));
  }
  const auto folder = picker.PickSingleFolder();
  if (folder) {
    co_return nlohmann::json {folder->string()};
  }
  co_return std::unexpected {std::string {"no folder was picked"}};
}

task<WebViewBasedSettingsPage::JSResponse>
WebViewBasedSettingsPage::JSAPI_NativeObjectToJSON(
  const nlohmann::json params) {
  auto self = this->get_strong();
  co_await wil::resume_foreground(DispatcherQueue());

  if (
    mPage->mInstanceID
    != winrt::guid {params.at("instanceID").get<std::string_view>()}) {
    co_return std::unexpected {std::string {"instanceID does not match"}};
  }

  co_return nlohmann::json(MetaInvoke(mPage, [](auto& native, auto js) {
    return js.MapProperties([&native](auto prop) {
      return std::pair {
        prop.GetName(),
        nlohmann::json(prop.Get(native)),
      };
    }) | std::ranges::to<std::unordered_map>();
  }));
}

#ifdef TODO_PORT_ME

static constexpr const char TriggeredCrashMessage[]
  = "'Trigger crash' clicked on developer tools page";

OpenKneeboard::fire_and_forget WebViewBasedSettingsPage::OnTriggerCrashClick(
  IInspectable,
  Microsoft::UI::Xaml::RoutedEventArgs) {
  enum class CrashKind {
    Fatal = 0,
    Throw = 1,
    ThrowFromNoexcept = 2,
  };
  enum class CrashLocation {
    UIThread = 0,
    MUITask = 1,
    WindowsSystemTask = 2,
  };

  // We need the unreachable co_return to make the functions coroutines, so we
  // get task<void>'s exception handler
  std::function<task<void>()> triggerCrash;
  switch (static_cast<CrashKind>(this->CrashKind().SelectedIndex())) {
    case CrashKind::Fatal:
      triggerCrash = []() -> task<void> {
        fatal("{}", TriggeredCrashMessage);
        co_return;
      };
      break;
    case CrashKind::Throw:
      triggerCrash = []() -> task<void> {
        throw std::runtime_error(TriggeredCrashMessage);
        co_return;
      };
      break;
    case CrashKind::ThrowFromNoexcept:
      triggerCrash = []() noexcept -> task<void> {
        throw std::runtime_error(TriggeredCrashMessage);
        co_return;
      };
      break;
    default:
      OPENKNEEBOARD_BREAK;
      // Error: task failed successfully 🤷
      fatal("Invalid CrashKind selected from dev tools page");
  }

  if (!triggerCrash) {
    OPENKNEEBOARD_BREAK;
    fatal("triggerCrash not set");
  }

  const auto location
    = static_cast<CrashLocation>(this->CrashLocation().SelectedIndex());
  if (location == CrashLocation::UIThread) {
    co_await triggerCrash();
    std::unreachable();
  }
  if (location == CrashLocation::MUITask) {
    auto dqc = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::
      CreateOnDedicatedThread();
    co_await wil::resume_foreground(dqc.DispatcherQueue());
    co_await triggerCrash();
    std::unreachable();
  }

  if (location == CrashLocation::WindowsSystemTask) {
    auto dqc = winrt::Windows::System::DispatcherQueueController::
      CreateOnDedicatedThread();
    co_await wil::resume_foreground(dqc.DispatcherQueue());
    co_await triggerCrash();
    std::unreachable();
  }

  OPENKNEEBOARD_BREAK;
  fatal(
    "Unhandled CrashLocation from devtools page: {}",
    std::to_underlying(location));
}
#endif
}// namespace winrt::OpenKneeboardApp::implementation