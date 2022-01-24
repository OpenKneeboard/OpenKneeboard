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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
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
