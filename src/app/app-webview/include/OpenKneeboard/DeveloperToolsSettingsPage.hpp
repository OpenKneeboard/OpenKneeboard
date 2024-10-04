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

#include "JSObject.hpp"

#include <OpenKneeboard/task.hpp>

namespace OpenKneeboard {

class KneeboardState;

struct JSNativeData {
  virtual ~JSNativeData() = default;
};

class DeveloperToolsSettingsPage : public JSNativeData {
 private:
  KneeboardState* mKneeboard {nullptr};

 public:
  DeveloperToolsSettingsPage() = delete;
  DeveloperToolsSettingsPage(KneeboardState*);
  ~DeveloperToolsSettingsPage() override = default;

  std::filesystem::path GetAppWebViewSourcePath() const;
  fire_and_forget SetAppWebViewSourcePath(std::filesystem::path);
  std::filesystem::path GetDefaultAppWebViewSourcePath() const;

  bool GetIsPluginFileTypeInHKCU() const;
  fire_and_forget SetIsPluginFileTypeInHKCU(bool);

  std::string GetAutoUpdateFakeCurrentVersion() const;
  fire_and_forget SetAutoUpdateFakeCurrentVersion(std::string);
  std::string GetActualCurrentVersion() const;

  fire_and_forget CopyAPIEventsToClipboard() const;
  fire_and_forget CopyDebugMessagesToClipboard() const;

  enum class CrashKind {
    Fatal,
    Throw,
    ThrowFromNoexcept,
    Terminate,
  };
  enum class CrashLocation {
    UIThread,
    MUITask,
    WindowsSystemTask,
  };

  fire_and_forget TriggerCrash(CrashKind, CrashLocation) const;
};

DECLARE_JS_MEMBER_ENUM(DeveloperToolsSettingsPage, CrashKind)
DECLARE_JS_MEMBER_ENUM(DeveloperToolsSettingsPage, CrashLocation)

DECLARE_JS_CLASS(DeveloperToolsSettingsPage) {
  DECLARE_JS_PROPERTIES {
    DECLARE_JS_PROPERTY(AppWebViewSourcePath),
    DECLARE_READ_ONLY_JS_PROPERTY(DefaultAppWebViewSourcePath),
    DECLARE_JS_PROPERTY(IsPluginFileTypeInHKCU),
    DECLARE_JS_PROPERTY(AutoUpdateFakeCurrentVersion),
    DECLARE_READ_ONLY_JS_PROPERTY(ActualCurrentVersion),
  };
  DECLARE_JS_METHODS {
    DECLARE_JS_METHOD(CopyAPIEventsToClipboard),
    DECLARE_JS_METHOD(CopyDebugMessagesToClipboard),
    DECLARE_JS_METHOD(TriggerCrash),
  };
};

}// namespace OpenKneeboard