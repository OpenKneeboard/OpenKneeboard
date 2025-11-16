// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

namespace OpenKneeboard {

enum class RenderMode {
  // Usually wanted for VR as we have our own swapchain for
  // our compositor layers
  ClearAndRender,
  // Usually wanted for non-VR as we're rendering on top of the
  // application's swapchain texture
  Overlay,
};

}