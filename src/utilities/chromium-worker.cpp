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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <Windows.h>

#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_sandbox_win.h>

namespace OpenKneeboard::Cef {

class BrowserApp final : public CefApp, public CefBrowserProcessHandler {
 public:
  BrowserApp() {
  }

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

 private:
  IMPLEMENT_REFCOUNTING(BrowserApp);
  DISALLOW_COPY_AND_ASSIGN(BrowserApp);
};

}// namespace OpenKneeboard::Cef

int __stdcall wWinMain(
  HINSTANCE hInstance,
  [[maybe_unused]] HINSTANCE hPrevInstance,
  [[maybe_unused]] PWSTR lpCmdLine,
  [[maybe_unused]] int nCmdShow) {
  void* sandboxInfo = nullptr;
#ifdef CEF_USE_SANDBOX
  CefScopedSandboxInfo scopedSandbox;
  sandboxInfo = scoped_sandbox.sandbox_info();
#endif

  // `lpCmdLine` is inconsistent in whether or not it includes argv[0]
  CefMainArgs mainArgs(hInstance);

  CefRefPtr<OpenKneeboard::Cef::BrowserApp> app {
    new OpenKneeboard::Cef::BrowserApp()};

  return CefExecuteProcess(mainArgs, app.get(), sandboxInfo);
}