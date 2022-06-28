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
#include <OVR_CAPI_D3D.h>
#include <OpenKneeboard/D3D11.h>
#include <OpenKneeboard/dprint.h>

#include "OVRProxy.h"
#include "OculusD3D11Kneeboard.h"

namespace OpenKneeboard {
bool OculusD3D11Kneeboard::Render(
  ID3D11Device* device,
  const std::vector<winrt::com_ptr<ID3D11RenderTargetView>>& renderTargetViews,
  ovrSession session,
  ovrTextureSwapChain swapChain,
  const SHM::Snapshot& snapshot,
  uint8_t layerIndex,
  const VRKneeboard::RenderParameters& params) {
  if (!swapChain) {
    dprint("asked to render without swapchain");
    return false;
  }

  if (!device) {
    dprintf(" - no D3D11 - {}", __FUNCTION__);
    return false;
  }

  auto ovr = OVRProxy::Get();
  const auto config = snapshot.GetConfig();

  auto sharedTexture = snapshot.GetLayerTexture(device, layerIndex);
  if (!sharedTexture.IsValid()) {
    dprint(" - invalid shared texture");
    return false;
  }

  int index = -1;
  ovr->ovr_GetTextureSwapChainCurrentIndex(session, swapChain, &index);
  if (index < 0) {
    dprintf(" - invalid swap chain index ({})", index);
    return false;
  }

  D3D11::CopyTextureWithOpacity(
    device,
    sharedTexture.GetShaderResourceView(),
    renderTargetViews.at(index).get(),
    params.mKneeboardOpacity);

  auto error = ovr->ovr_CommitTextureSwapChain(session, swapChain);
  if (error) {
    dprintf("Commit failed with {}", error);
  }

  return true;
}

}// namespace OpenKneeboard
