#pragma once

#include <filesystem>

namespace OpenKneeboard::RuntimeFiles {

extern const std::filesystem::path DCSWORLD_HOOK_DLL;
extern const std::filesystem::path DCSWORLD_HOOK_LUA;


extern const std::filesystem::path AUTOINJECT_MARKER_DLL;
extern const std::filesystem::path NON_VR_D3D11_DLL;
extern const std::filesystem::path OCULUS_D3D11_DLL;
extern const std::filesystem::path OCULUS_D3D12_DLL;
extern const std::filesystem::path INJECTION_BOOTSTRAPPER_DLL;

}// namespace OpenKneeboard::RuntimeFiles
