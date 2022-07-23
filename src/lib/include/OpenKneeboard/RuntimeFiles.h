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

#include <shims/filesystem>

namespace OpenKneeboard::RuntimeFiles {

#define OPENKNEEBOARD_RUNTIME_FILES \
  IT(DCSWORLD_HOOK_DLL) \
  IT(DCSWORLD_HOOK_LUA) \
  IT(AUTOINJECT_MARKER_DLL) \
  IT(INJECTION_BOOTSTRAPPER_DLL) \
  IT(TABLET_PROXY_DLL) \
  IT(NON_VR_D3D11_DLL) \
  IT(OCULUS_D3D11_DLL) \
  IT(OCULUS_D3D12_DLL) \
  IT(OPENXR_DLL) \
  IT(OPENXR_JSON) \
  IT(OPENXR_REGISTER_LAYER_HELPER) \
  IT(QUICK_START_PDF) \
  IT(WINDOWSAPP_LAUNCHER)

#define IT(x) extern const std::filesystem::path x;
OPENKNEEBOARD_RUNTIME_FILES
#undef IT

/** Installs to `GetDirectory()`, or throws */
void Install();

std::filesystem::path GetDirectory();

}// namespace OpenKneeboard::RuntimeFiles
