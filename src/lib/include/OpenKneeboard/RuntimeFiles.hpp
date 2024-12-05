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

#include <filesystem>

namespace OpenKneeboard::RuntimeFiles {

#ifdef _WIN64
#define OPENKNEEBOARD_PRIVATE_ARCH_RUNTIME_FILES \
  IT(WINDOW_CAPTURE_HOOK_32BIT_HELPER)
#else
#define OPENKNEEBOARD_PRIVATE_ARCH_RUNTIME_FILES
#endif

#define OPENKNEEBOARD_PRIVATE_RUNTIME_FILES \
  IT(OPENXR_REGISTER_LAYER_HELPER) \
  IT(SET_DESIRED_ELEVATION_HELPER) \
  OPENKNEEBOARD_PRIVATE_ARCH_RUNTIME_FILES

#define OPENKNEEBOARD_PUBLIC_RUNTIME_FILES \
  IT(AUTODETECTION_DLL) \
  IT(DCSWORLD_HOOK_DLL) \
  IT(DCSWORLD_HOOK_LUA) \
  IT(TABLET_PROXY_DLL) \
  IT(WINDOW_CAPTURE_HOOK_DLL) \
  IT(NON_VR_D3D11_DLL) \
  IT(OCULUS_D3D11_DLL) \
  IT(OPENXR_64BIT_DLL) \
  IT(OPENXR_32BIT_DLL) \
  IT(OPENXR_64BIT_JSON) \
  IT(OPENXR_32BIT_JSON) \
  IT(QUICK_START_PDF)

#define OPENKNEEBOARD_RUNTIME_FILES \
  OPENKNEEBOARD_PUBLIC_RUNTIME_FILES \
  OPENKNEEBOARD_PRIVATE_RUNTIME_FILES

#define IT(x) extern const std::filesystem::path x;
OPENKNEEBOARD_RUNTIME_FILES
#undef IT

std::filesystem::path GetInstallationDirectory();

}// namespace OpenKneeboard::RuntimeFiles
