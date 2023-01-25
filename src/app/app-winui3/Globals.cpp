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
// clang-format off
#include "pch.h"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.h>
#include <OpenKneeboard/TroubleshootingStore.h>

namespace OpenKneeboard {

HWND gMainWindow {};
std::shared_ptr<KneeboardState> gKneeboard;
std::shared_ptr<TroubleshootingStore> gTroubleshootingStore;
DXResources gDXResources;
winrt::handle gMutex {};
std::vector<winrt::weak_ref<winrt::OpenKneeboardApp::TabPage>> gTabs;
RenderTargetID gGUIRenderTargetID;

}// namespace OpenKneeboard
