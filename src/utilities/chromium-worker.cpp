/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */
#include <OpenKneeboard/Filesystem.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/fatal.hpp>
#include <OpenKneeboard/format/filesystem.hpp>

#include <Windows.h>

#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_sandbox_win.h>

#include <fstream>
#include <unordered_map>

namespace OpenKneeboard {
/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.ChromiumWorker")
 * c7ba8cbb-cc1f-5c43-e114-a837f6b5ae95
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.ChromiumWorker",
  (0xc7ba8cbb, 0xcc1f, 0x5c43, 0xe1, 0x14, 0xa8, 0x37, 0xf6, 0xb5, 0xae, 0x95));
}// namespace OpenKneeboard

namespace OpenKneeboard::Cef {

namespace {
CefString GetOKBNativeJS() {
  static CefString sRet;
  static std::once_flag sOnce;
  std::call_once(sOnce, [&ret = sRet]() {
    // 'libexec/cef/' = 'share/'
    const auto directory
      = Filesystem::GetRuntimeDirectory().parent_path().parent_path() / "share";
    const auto file = directory / "Chromium.js";
    dprint("Loading JS from file: {}", file);
    std::ifstream f(file);
    std::stringstream ss;
    ss << f.rdbuf();
    sRet.FromString(ss.str());
  });
  return sRet;
}

}// namespace

class BrowserApp final : public CefApp,
                         public CefBrowserProcessHandler,
                         public CefRenderProcessHandler,
                         public CefV8Handler {
 public:
  BrowserApp() {
  }

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  void OnBrowserCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDictionaryValue> extraInfo) override {
    OPENKNEEBOARD_TraceLoggingScope("OnBrowserCreated()");
    mInitData.emplace(
      browser->GetIdentifier(), extraInfo->GetString("InitData"));
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override {
    OPENKNEEBOARD_TraceLoggingScope("OnBrowserDestroyed");
    mInitData.erase(browser->GetIdentifier());
  }

  void OnWebKitInitialized() override {
    OPENKNEEBOARD_TraceLoggingScope("OnWebKitInitialized");
    CefRegisterExtension("OpenKneeboard/Native", GetOKBNativeJS(), this);
  }

  void OnContextReleased(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame>,
    CefRefPtr<CefV8Context>) override {
    OPENKNEEBOARD_TraceLoggingScope("OnContextReleased");
    const auto id = browser->GetIdentifier();
    if (mJSMessageHandlers.contains(id)) {
      mJSMessageHandlers.erase(id);
    }
  }

  bool OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId sourceProcess,
    CefRefPtr<CefProcessMessage> message) override {
    const auto name = message->GetName().ToString();
    if (name == "okb/asyncResponse") {
      if (!mJSMessageHandlers.contains(browser->GetIdentifier())) {
        return true;
      }
      auto& [context, handler]
        = mJSMessageHandlers.at(browser->GetIdentifier());
      if (context->Enter()) {
        const auto args = message->GetArgumentList();

        CefV8ValueList jsArgs;
        jsArgs.push_back(CefV8Value::CreateString(message->GetName()));
        jsArgs.push_back(CefV8Value::CreateInt(args->GetInt(0)));// ID
        jsArgs.push_back(
          CefV8Value::CreateString(args->GetString(1)));// details
        handler->ExecuteFunction(nullptr, jsArgs);

        context->Exit();
      }
      return true;
    }

    return CefRenderProcessHandler::OnProcessMessageReceived(
      browser, frame, sourceProcess, message);
  }

  bool Execute(
    const CefString& name,
    CefRefPtr<CefV8Value> object,
    const CefV8ValueList& arguments,
    CefRefPtr<CefV8Value>& ret,
    CefString& exception) {
    OPENKNEEBOARD_TraceLoggingScope(
      "Execute/V8", TraceLoggingValue(name.ToString().c_str(), "name"));

    const auto browser = CefV8Context::GetCurrentContext()->GetBrowser();

    if (name == "OKBNative_GetInitializationData") {
      return JSGetInitializationData(browser, ret);
    }
    if (name == "OKBNative_SendMessage") {
      return JSSendMessage(browser, arguments);
    }
    if (name == "OKBNative_OnMessage") {
      return JSOnMessage(browser, arguments);
    }

    dprint.Warning("Unrecognized v8 function: {}");
    return false;
  }

  [[nodiscard]]
  bool JSGetInitializationData(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefV8Value>& ret) {
    OPENKNEEBOARD_TraceLoggingScope("JSGetInitializationData");

    const auto browserID = browser->GetIdentifier();
    if (!mInitData.contains(browserID)) {
      dprint.Warning("Unrecognized browser ID");
      return false;
    }

    ret = CefV8Value::CreateString(mInitData.at(browserID));
    return true;
  }

  [[nodiscard]]
  bool JSSendMessage(
    CefRefPtr<CefBrowser> browser,
    const CefV8ValueList& arguments) {
    OPENKNEEBOARD_TraceLoggingScope("JSSendMessage");

    auto message = CefProcessMessage::Create(arguments.at(0)->GetStringValue());
    message->GetArgumentList()->SetInt(0, arguments.at(1)->GetIntValue());// ID
    message->GetArgumentList()->SetString(
      1, arguments.at(2)->GetStringValue());// data
    browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, message);
    return true;
  }

  [[discard]]
  bool JSOnMessage(
    CefRefPtr<CefBrowser> browser,
    const CefV8ValueList& arguments) {
    const auto id = browser->GetIdentifier();
    mJSMessageHandlers.insert_or_assign(
      id, std::tuple {CefV8Context::GetCurrentContext(), arguments.at(0)});
    return true;
  }

 private:
  IMPLEMENT_REFCOUNTING(BrowserApp);
  DISALLOW_COPY_AND_ASSIGN(BrowserApp);

  std::unordered_map<int, CefString> mInitData;
  std::unordered_map<
    int,
    std::tuple<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value>>>
    mJSMessageHandlers;
};

}// namespace OpenKneeboard::Cef

using namespace OpenKneeboard;

int __stdcall wWinMain(
  HINSTANCE hInstance,
  [[maybe_unused]] HINSTANCE hPrevInstance,
  [[maybe_unused]] PWSTR lpCmdLine,
  [[maybe_unused]] int nCmdShow) {
  TraceLoggingRegister(gTraceProvider);
  const scope_exit unregisterTraceProvider(
    []() { TraceLoggingUnregister(gTraceProvider); });
  divert_process_failure_to_fatal();

  DPrintSettings::Set({
    .prefix = "OpenKneeboard-Chromium",
  });

  void* sandboxInfo = nullptr;
#ifdef CEF_USE_SANDBOX
  CefScopedSandboxInfo scopedSandbox;
  sandboxInfo = scoped_sandbox.sandbox_info();
#endif

  // `lpCmdLine` is inconsistent in whether or not it includes argv[0]
  CefMainArgs mainArgs(hInstance);

  CefRefPtr<CefApp> app {new OpenKneeboard::Cef::BrowserApp()};

  return CefExecuteProcess(mainArgs, app.get(), sandboxInfo);
}