// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
// clang-format on

#include "Globals.h"

#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/TroubleshootingStore.hpp>

namespace OpenKneeboard {

HWND gMainWindow {};
std::weak_ptr<TroubleshootingStore> gTroubleshootingStore;
audited_weak_ptr<KneeboardState> gKneeboard;
audited_ptr<DXResources> gDXResources;
winrt::handle gMutex {};
RenderTargetID gGUIRenderTargetID;
bool gShuttingDown = false;

}// namespace OpenKneeboard
