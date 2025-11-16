// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/RenderTargetID.hpp>

#include <winrt/OpenKneeboardApp.h>

#include <OpenKneeboard/audited_ptr.hpp>

#include <memory>
#include <vector>

namespace OpenKneeboard {
class KneeboardState;
class TroubleshootingStore;

extern HWND gMainWindow;
extern std::weak_ptr<KneeboardState> gKneeboard;
extern audited_ptr<DXResources> gDXResources;
extern winrt::handle gMutex;
extern std::weak_ptr<TroubleshootingStore> gTroubleshootingStore;
extern std::vector<winrt::weak_ref<winrt::OpenKneeboardApp::TabPage>> gTabs;
extern RenderTargetID gGUIRenderTargetID;
extern bool gShuttingDown;

}// namespace OpenKneeboard
