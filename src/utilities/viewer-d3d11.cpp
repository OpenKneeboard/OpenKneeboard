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

#include "viewer-d3d11.h"

#include <OpenKneeboard/D3D11.h>

#include <OpenKneeboard/hresult.h>

#include <directxtk/ScreenGrab.h>

namespace OpenKneeboard::Viewer {

D3D11Renderer::D3D11Renderer(const winrt::com_ptr<ID3D11Device>& device) {
  dprint(__FUNCSIG__);
  mD3D11Device = device.as<ID3D11Device1>();
  device->GetImmediateContext(mD3D11ImmediateContext.put());

  mSpriteBatch = std::make_unique<D3D11::SpriteBatch>(device.get());
}

D3D11Renderer::~D3D11Renderer() = default;

std::wstring_view D3D11Renderer::GetName() const noexcept {
  return {L"D3D11"};
}

SHM::CachedReader* D3D11Renderer::GetSHM() {
  return &mSHM;
}

void D3D11Renderer::Initialize(uint8_t swapchainLength) {
  mSHM.InitializeCache(mD3D11Device.get(), swapchainLength);
}

uint64_t D3D11Renderer::Render(
  SHM::IPCClientTexture* sourceTexture,
  const PixelRect& sourceRect,
  HANDLE destTextureHandle,
  const PixelSize& destTextureDimensions,
  const PixelRect& destRect,
  [[maybe_unused]] HANDLE fence,
  uint64_t fenceValueIn) {
  OPENKNEEBOARD_TraceLoggingScope("Viewer::D3D11Renderer::Render");
  if (mDestDimensions != destTextureDimensions) {
    mDestHandle = {};
  }
  if (mSessionID != mSHM.GetSessionID()) {
    mDestHandle = {};
  }
  if (mDestHandle != destTextureHandle) {
    mDestTexture = nullptr;
    mDestRenderTargetView = nullptr;
    check_hresult(mD3D11Device->OpenSharedResource1(
      destTextureHandle, IID_PPV_ARGS(mDestTexture.put())));

    check_hresult(mD3D11Device->CreateRenderTargetView(
      mDestTexture.get(), nullptr, mDestRenderTargetView.put()));
    mDestHandle = destTextureHandle;
    mDestDimensions = destTextureDimensions;
  }

  auto sourceSRV = reinterpret_cast<SHM::D3D11::Texture*>(sourceTexture)
                     ->GetD3D11ShaderResourceView();

  // We could just use CopySubResourceRegion(), but we might as test
  // D3D11::SpriteBatch a little :)
  //
  // It will also be more consistent with the other viewer
  // renderers.
  mSpriteBatch->Begin(mDestRenderTargetView.get(), destTextureDimensions);
  mSpriteBatch->Draw(sourceSRV, sourceRect, destRect);
  mSpriteBatch->End();

  // No need for a fence wait with D3D11
  return fenceValueIn;
}

void D3D11Renderer::SaveTextureToFile(
  SHM::IPCClientTexture* texture,
  const std::filesystem::path& path) {
  check_hresult(DirectX::SaveDDSTextureToFile(
    mD3D11ImmediateContext.get(),
    reinterpret_cast<SHM::D3D11::Texture*>(texture)->GetD3D11Texture(),
    path.wstring().c_str()));
}

}// namespace OpenKneeboard::Viewer