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

#include <OpenKneeboard/Plugin.hpp>

namespace OpenKneeboard {

class KneeboardState;

class PluginsSettingsPage : public JSNativeData {
 private:
  KneeboardState* mKneeboard {nullptr};

 public:
  PluginsSettingsPage() = delete;
  PluginsSettingsPage(KneeboardState*);
  ~PluginsSettingsPage() override = default;
};

DECLARE_JS_CLASS(PluginsSettingsPage) {};

DECLARE_JS_STRUCT_MEMBER_STRUCT(
  Plugin,
  Metadata,
  mPluginName,
  mPluginReadableVersion);

}// namespace OpenKneeboard