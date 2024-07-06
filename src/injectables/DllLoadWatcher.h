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
