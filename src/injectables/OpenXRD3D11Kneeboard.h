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

#include <OpenKneeboard/config.h>

#include "OpenXRKneeboard.h"

namespace OpenKneeboard {

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(
    XrSession,
    const std::shared_ptr<OpenXRNext>&,
    ID3D11Device*);
  ~OpenXRD3D11Kneeboard();

 protected:
  virtual XrSwapchain CreateSwapChain(XrSession, uint8_t layerIndex) override;
  virtual bool Render(
    XrSwapchain swapchain,
    const SHM::Snapshot& snapshot,
    uint8_t layerIndex,
    const VRKneeboard::RenderParameters&) override;

 private:
  ID3D11Device* mDevice = nullptr;

  std::array<std::vector<winrt::com_ptr<ID3D11RenderTargetView>>, MaxLayers>
    mRenderTargetViews;
};

}// namespace OpenKneeboard
