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

void ReadJSFile(CefString& ret, auto name) {
  // 'libexec/cef/' = 'share/'
  const auto directory
    = Filesystem::GetRuntimeDirectory().parent_path().parent_path() / "share";

  std::stringstream ss;
  std::ifstream f(directory / name);
  ss << f.rdbuf();
  ret.FromString(ss.str());
}

CefString GetOpenKneeboardNativeJS() {
  static CefString sRet;
  static std::once_flag sOnce;
  std::call_once(
    sOnce, [&ret = sRet]() { ReadJSFile(ret, "OpenKneeboardNative.js"); });
  return sRet;
}

CefString GetOpenKneeboardAPIJS() {
  static CefString sRet;
  static std::once_flag sOnce;
  std::call_once(
    sOnce, [&ret = sRet]() { ReadJSFile(ret, "OpenKneeboardAPI.js"); });
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
    mBrowserData.emplace(
      browser->GetIdentifier(),
      BrowserData {
        .mInitializationData = extraInfo->GetString("InitData"),
      });
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override {
    OPENKNEEBOARD_TraceLoggingScope("OnBrowserDestroyed");
    const auto id = browser->GetIdentifier();
    if (mBrowserData.contains(id)) {
      mBrowserData.erase(id);
    }
  }

  void OnWebKitInitialized() override {
    OPENKNEEBOARD_TraceLoggingScope("OnWebKitInitialized");
    CefRegisterExtension(
      "OpenKneeboard/Native", GetOpenKneeboardNativeJS(), this);
  }

  void OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) override {
    OPENKNEEBOARD_TraceLoggingScope("OnContextCreated");

    if (!frame->GetV8Context()->IsSame(context)) {
      // Secondary context, e.g. dev tools window
      //
      // See https://github.com/chromiumembedded/cef/issues/3867
      return;
    }
    if (!frame->IsMain()) {
      return;
    }

    const auto window = context->GetGlobal();

    if (!context->Enter()) {
      return;
    }
    const scope_exit exitContext([context] { context->Exit(); });

    CefRefPtr<CefV8Value> ret;
    CefRefPtr<CefV8Exception> exception;
    context->Eval(
      GetOpenKneeboardAPIJS(), "OpenKneeboardAPI.js", 1, ret, exception);
    context->Eval(
      "new OpenKneeboardAPI()", "OpenKneeboardInit.js", 1, ret, exception);
    window->SetValue("OpenKneeboard", ret, V8_PROPERTY_ATTRIBUTE_READONLY);
    OPENKNEEBOARD_ALWAYS_ASSERT(window->HasValue("OpenKneeboard"));
  }

  void OnContextReleased(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) override {
    OPENKNEEBOARD_TraceLoggingScope("OnContextReleased");
    if (!frame->GetV8Context()->IsSame(context)) {
      return;
    }
    if (!frame->IsMain()) {
      return;
    }

    dprint("OnContextReleased");
    const auto id = browser->GetIdentifier();
    if (mBrowserData.contains(id)) {
      mBrowserData.erase(id);
    }
  }

  bool OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId sourceProcess,
    CefRefPtr<CefProcessMessage> message) override {
    OPENKNEEBOARD_TraceLoggingScope("OnProcessMessageReceived()");

    const auto name = message->GetName().ToString();
    if (name == "okb/asyncResult") {
      auto& state = mBrowserData.at(browser->GetIdentifier());
      const auto args = message->GetArgumentList();
      const auto id = args->GetInt(0);

      if (!state.mJSPromises.contains(id)) {
        dprint.Warning("Could not find JS promise with ID {}", id);
        return true;
      }
      auto& [context, promise] = state.mJSPromises.at(id);
      if (context->Enter()) {
        CefV8ValueList jsArgs;
        promise->ResolvePromise(CefV8Value::CreateString(args->GetString(1)));
        context->Exit();

        state.mJSPromises.erase(id);
      }
      return true;
    }

    constexpr std::string_view EventPrefix = "okbEvent/";
    if (name.starts_with(EventPrefix)) {
      const auto eventName = name.substr(EventPrefix.length());

      auto& state = mBrowserData.at(browser->GetIdentifier());
      const auto args = message->GetArgumentList();
      for (auto&& [context, callback]: state.mEventCallbacks) {
        CefV8ValueList jsArgs;
        jsArgs.push_back(CefV8Value::CreateString(eventName));
        for (std::size_t i = 0; i < args->GetSize(); ++i) {
          if (args->GetType(i) != CefValueType::VTYPE_STRING) {
            dprint.Warning("JS event {} has non-string arg {}", eventName, i);
            return true;
          }
          jsArgs.push_back(CefV8Value::CreateString(args->GetString(i)));
        }
        callback->ExecuteFunctionWithContext(context, nullptr, jsArgs);
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
    if (name == "OKBNative_AsyncRequest") {
      return JSAsyncRequest(browser, arguments, ret);
    }
    if (name == "OKBNative_AddEventCallback") {
      return JSAddEventCallback(browser, arguments);
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
    if (!mBrowserData.contains(browserID)) {
      dprint.Warning("Unrecognized browser ID");
      return false;
    }

    ret = CefV8Value::CreateString(
      mBrowserData.at(browserID).mInitializationData);
    return true;
  }

  [[nodiscard]]
  bool JSAddEventCallback(
    CefRefPtr<CefBrowser> browser,
    const CefV8ValueList& arguments) {
    OPENKNEEBOARD_TraceLoggingScope("JSAddEventCallback");
    auto& state = mBrowserData.at(browser->GetIdentifier());
    state.mEventCallbacks.push_back({
      CefV8Context::GetCurrentContext(),
      arguments.at(0),
    });

    return true;
  }

  [[nodiscard]]
  bool JSAsyncRequest(
    CefRefPtr<CefBrowser> browser,
    const CefV8ValueList& arguments,
    CefRefPtr<CefV8Value>& ret) {
    OPENKNEEBOARD_TraceLoggingScope("JSAsyncRequest");

    auto& state = mBrowserData.at(browser->GetIdentifier());

    const auto promiseID = state.mNextPromiseID++;
    ret = CefV8Value::CreatePromise();
    state.mJSPromises.emplace(
      promiseID, std::tuple {CefV8Context::GetCurrentContext(), ret});

    auto message = CefProcessMessage::Create(arguments.at(0)->GetStringValue());
    message->GetArgumentList()->SetInt(0, promiseID);
    message->GetArgumentList()->SetString(1, arguments.at(1)->GetStringValue());
    browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, message);

    return true;
  }

 private:
  IMPLEMENT_REFCOUNTING(BrowserApp);
  DISALLOW_COPY_AND_ASSIGN(BrowserApp);

  struct BrowserData {
    int mNextPromiseID {};
    CefString mInitializationData;
    std::vector<std::tuple<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value>>>
      mEventCallbacks;
    std::unordered_map<
      int,
      std::tuple<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value>>>
      mJSPromises;
  };
  std::unordered_map<int, BrowserData> mBrowserData;
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