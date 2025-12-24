// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <filesystem>
#include <string_view>

namespace OpenKneeboard::RuntimeFiles {

#ifdef _WIN64
#define OPENKNEEBOARD_PRIVATE_ARCH_RUNTIME_FILES \
  IT(WINDOW_CAPTURE_HOOK_32BIT_HELPER)
#else
#define OPENKNEEBOARD_PRIVATE_ARCH_RUNTIME_FILES
#endif

#define OPENKNEEBOARD_PRIVATE_RUNTIME_FILES \
  IT(OPENXR_REGISTER_LAYER_HELPER) \
  OPENKNEEBOARD_PRIVATE_ARCH_RUNTIME_FILES

#define OPENKNEEBOARD_PUBLIC_RUNTIME_FILES \
  IT(CHROMIUM) \
  IT(DCSWORLD_HOOK_DLL) \
  IT(DCSWORLD_HOOK_LUA) \
  IT(WINDOW_CAPTURE_HOOK_DLL) \
  IT(OPENXR_64BIT_DLL) \
  IT(OPENXR_32BIT_DLL) \
  IT(OPENXR_64BIT_JSON) \
  IT(OPENXR_32BIT_JSON) \
  IT(QUICK_START_PDF)

#define OPENKNEEBOARD_RUNTIME_FILES \
  OPENKNEEBOARD_PUBLIC_RUNTIME_FILES \
  OPENKNEEBOARD_PRIVATE_RUNTIME_FILES

#define IT(x) extern const std::string_view x;
OPENKNEEBOARD_RUNTIME_FILES
#undef IT

std::filesystem::path GetInstallationDirectory();

}// namespace OpenKneeboard::RuntimeFiles
