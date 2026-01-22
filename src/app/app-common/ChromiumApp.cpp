// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ChromiumApp.hpp>
#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/version.hpp>

#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_version.h>

#ifdef CEF_USE_SANDBOX
#include <include/cef_sandbox_win.h>
#endif

namespace OpenKneeboard {

class ChromiumApp::Impl : public CefApp, public CefBrowserProcessHandler {
 public:
  Impl() {}

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnBeforeCommandLineProcessing(
    const CefString& /*process_type*/,
    CefRefPtr<CefCommandLine> command_line) override {
    command_line->AppendSwitch("angle");
    command_line->AppendSwitchWithValue("use-angle", "d3d11");
    command_line->AppendSwitchWithValue("use-adapter-luid", this->GetGpuLuid());
  }

 private:
  IMPLEMENT_REFCOUNTING(Impl);
  DISALLOW_COPY_AND_ASSIGN(Impl);

  CefString mGpuLuid;

  CefString GetGpuLuid() {
    if (!mGpuLuid.empty()) {
      return mGpuLuid;
    }
    D3D11Resources dxr;
    const auto luid = std::bit_cast<uint64_t>(dxr.mAdapterLUID);
    dprint("Setting CEF GPU LUID to {:#018x}", luid);
    mGpuLuid = std::format(
      "{},{}", luid >> 32, luid & std::numeric_limits<uint32_t>::max());
    return mGpuLuid;
  }
};

struct ChromiumApp::Wrapper {
  struct CefApp;

  CefRefPtr<Impl> mCefApp;
  void* mSandbox {nullptr};
};

ChromiumApp::ChromiumApp(const HINSTANCE instance, void* sandbox) {
  p.reset(new Wrapper {
    .mCefApp = new Impl(),
    .mSandbox = sandbox,
  });

  const CefMainArgs mainArgs {instance};
  CefSettings settings {};
  CefString(&settings.log_file)
    .FromWString(
      (Filesystem::GetLogsDirectory() / "chromium-debug.log").wstring());
  settings.log_severity = cef_log_severity_t::LOGSEVERITY_ERROR;
  settings.multi_threaded_message_loop = true;
  settings.windowless_rendering_enabled = true;
  CefString(&settings.user_agent_product)
    .FromString(
      std::format(
        "OpenKneeboard/{}.{}.{}.{} Chromium/{}.0.0.0",
        Version::Major,
        Version::Minor,
        Version::Patch,
        Version::Build,
        CEF_VERSION_MAJOR));
  CefString(&settings.root_cache_path)
    .FromWString(
      (Filesystem::GetLocalAppDataDirectory() / "Chromium").wstring());

#ifndef CEF_USE_SANDBOX
  settings.no_sandbox = 1;
#endif

  CefInitialize(mainArgs, settings, p->mCefApp, p->mSandbox);
}

ChromiumApp::~ChromiumApp() {
  p = nullptr;
  CefShutdown();
}

void* ChromiumApp::GetSandbox() {
#ifndef CEF_USE_SANDBOX
  return nullptr;
#else
  static const CefScopedSandboxInfo sandbox;
  return sandbox.sandbox_info();
#endif
}

}// namespace OpenKneeboard
