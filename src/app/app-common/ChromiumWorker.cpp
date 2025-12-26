// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/CEF/JSSources.hpp>
#include <OpenKneeboard/ChromiumWorker.hpp>

#include <Windows.h>

#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_sandbox_win.h>

#include <unordered_map>

namespace OpenKneeboard::CEF {

namespace {

CefString GetOpenKneeboardNativeJS() {
  return JSSources::Get().OpenKneeboardNative<CefString>();
}

CefString GetOpenKneeboardAPIJS() {
  return JSSources::Get().OpenKneeboardAPI<CefString>();
}

CefString GetSimHubJS() {
  return JSSources::Get().SimHub<CefString>();
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
    mBrowserData.emplace(
      browser->GetIdentifier(),
      BrowserData {
        .mInitializationData = extraInfo->GetString("InitData"),
        .mIntegrateWithSimHub = extraInfo->GetBool("IntegrateWithSimHub"),
        .mExposeOpenKneeboardAPIs
        = extraInfo->GetBool("ExposeOpenKneeboardAPIs"),
      });
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override {
    const auto id = browser->GetIdentifier();
    if (mBrowserData.contains(id)) {
      mBrowserData.erase(id);
    }
  }

  void OnWebKitInitialized() override {
    CefRegisterExtension(
      "OpenKneeboard/Native", GetOpenKneeboardNativeJS(), this);
  }

  void OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) override {
    if (!frame->GetV8Context()->IsSame(context)) {
      // Secondary context, e.g. dev tools window
      //
      // See https://github.com/chromiumembedded/cef/issues/3867
      return;
    }
    if (!frame->IsMain()) {
      return;
    }

    const auto browserId = browser->GetIdentifier();
    if (!mBrowserData.contains(browserId)) {
      return;
    }
    auto& data = mBrowserData.at(browserId);
    if (!context->Enter()) {
      return;
    }
    const struct exit_context_t {
      using T = decltype(context.get());
      T mContext {};

      ~exit_context_t() {
        mContext->Exit();
      }
    } exitContext {context.get()};
    CefRefPtr<CefV8Value> ret;
    CefRefPtr<CefV8Exception> exception;

    if (!data.mExposeOpenKneeboardAPIs) {
      context->Eval(
        "console.warn('OpenKneeboard JS APIs are disabled by user settings');",
        {},
        1,
        ret,
        exception);
      return;
    }
    data.mJS.mMainWorldContext = context;

    context->Eval(
      GetOpenKneeboardAPIJS(),
      "https://openkneeboard.local/OpenKneeboardAPI.js",
      1,
      ret,
      exception);
    context->Eval(
      "new OpenKneeboardAPI()",
      "https://openkneeboard.local/OpenKneeboardInit.js",
      1,
      ret,
      exception);

    const auto window = context->GetGlobal();
    window->SetValue("OpenKneeboard", ret, V8_PROPERTY_ATTRIBUTE_READONLY);

    if (data.mIntegrateWithSimHub) {
      context->Eval(
        GetSimHubJS(),
        "https://openkneeboard.local/simhub.js",
        1,
        ret,
        exception);
    }
  }

  void OnContextReleased(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) override {
    const auto id = browser->GetIdentifier();
    if (!mBrowserData.contains(id)) {
      return;
    }
    auto& data = mBrowserData.at(id);
    const auto isMainWorldContext = data.mJS.mMainWorldContext
      && context->IsSame(data.mJS.mMainWorldContext);
    if (!isMainWorldContext) {
      return;
    }
    data.mJS = {};
    frame->SendProcessMessage(
      PID_BROWSER, CefProcessMessage::Create("okb/onContextReleased"));
  }

  bool OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId sourceProcess,
    CefRefPtr<CefProcessMessage> message) override {
    const auto name = message->GetName().ToString();
    if (name == "okb/asyncResult") {
      auto& state = mBrowserData.at(browser->GetIdentifier()).mJS;
      const auto args = message->GetArgumentList();
      const auto id = args->GetInt(0);

      if (!state.mPromises.contains(id)) {
        return true;
      }
      auto& [context, promise] = state.mPromises.at(id);
      if (context->Enter()) {
        CefV8ValueList jsArgs;
        promise->ResolvePromise(CefV8Value::CreateString(args->GetString(1)));
        context->Exit();

        state.mPromises.erase(id);
      }
      return true;
    }

    constexpr std::string_view EventPrefix = "okbEvent/";
    if (name.starts_with(EventPrefix)) {
      const auto eventName = name.substr(EventPrefix.length());

      auto& state = mBrowserData.at(browser->GetIdentifier()).mJS;
      const auto args = message->GetArgumentList();
      for (auto&& [context, callback]: state.mEventCallbacks) {
        CefV8ValueList jsArgs;
        jsArgs.push_back(CefV8Value::CreateString(eventName));
        for (std::size_t i = 0; i < args->GetSize(); ++i) {
          if (args->GetType(i) != CefValueType::VTYPE_STRING) {
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
    CefString& exception) override {
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

    return false;
  }

  [[nodiscard]]
  bool JSGetInitializationData(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefV8Value>& ret) {
    const auto browserID = browser->GetIdentifier();
    if (!mBrowserData.contains(browserID)) {
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
    auto& state = mBrowserData.at(browser->GetIdentifier()).mJS;
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
    auto& state = mBrowserData.at(browser->GetIdentifier()).mJS;

    const auto promiseID = state.mNextPromiseID++;
    ret = CefV8Value::CreatePromise();
    state.mPromises.emplace(
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
    struct JSData {
      int mNextPromiseID {};
      std::vector<std::tuple<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value>>>
        mEventCallbacks;
      std::unordered_map<
        int,
        std::tuple<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value>>>
        mPromises;
      CefRefPtr<CefV8Context> mMainWorldContext;
    };

    CefString mInitializationData;
    bool mIntegrateWithSimHub {false};
    bool mExposeOpenKneeboardAPIs {false};
    JSData mJS {};
  };
  std::unordered_map<int, BrowserData> mBrowserData;
};

}// namespace OpenKneeboard::CEF

namespace OpenKneeboard {
int ChromiumWorkerMain(HINSTANCE instance, void* sandbox) {
  const CefMainArgs mainArgs(instance);
  const CefRefPtr<CefApp> app {new OpenKneeboard::CEF::BrowserApp()};

  return CefExecuteProcess(mainArgs, app.get(), sandbox);
}
}// namespace OpenKneeboard

/** Prefer discrete GPU.
 *
 * As we're using OnAcceleratedPaint, we need to be on the same GPU as the main
 * process for the texture to be usable, which needs to be the same GPU as the
 * VR headset. This will pretty much always be the 'high performance' gpu.
 */
extern "C" {
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

// http://developer.amd.com/community/blog/2015/10/02/amd-enduro-system-for-developers/
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}