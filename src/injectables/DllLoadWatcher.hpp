// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace OpenKneeboard {

/** Trigger a callback when a DLL is loaded.
 *
 * This has inherent race conditions, as another thread could
 * load a library in between checking if it's present, and
 * installing the hook.
 *
 * Recommended usage is to make your `onDllLoaded` callback:
 * - guard itself with a mutex
 * - handle multiple calls
 * - check if the DLL is loaded
 */
class DllLoadWatcher final {
 public:
  DllLoadWatcher() = delete;

  DllLoadWatcher(std::string_view name);
  ~DllLoadWatcher();

  bool IsDllLoaded() const;

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<void()> onDllLoaded;
  };

  void InstallHook(const Callbacks&);
  void UninstallHook();

  const char* GetDLLName() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> p;
};

}// namespace OpenKneeboard
