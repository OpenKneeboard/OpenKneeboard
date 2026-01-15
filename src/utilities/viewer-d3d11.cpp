// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "viewer-d3d11.hpp"

#include <OpenKneeboard/D3D11.hpp>

#include <OpenKneeboard/hresult.hpp>

#include <directxtk/ScreenGrab.h>

namespace OpenKneeboard::Viewer {

D3D11Renderer::D3D11Renderer(const winrt::com_ptr<ID3D11Device>& device) {
  dprint(__FUNCSIG__);
  mD3D11Device = device.as<ID3D11Device1>();
  device->GetImmediateContext(mD3D11ImmediateContext.put());

  mSpriteBatch = std::make_unique<D3D11::SpriteBatch>(device.get());

  mSHM = std::make_unique<SHM::D3D11::Reader>(
    SHM::ConsumerKind::Viewer, device.get());
}

D3D11Renderer::~D3D11Renderer() = default;

std::wstring_view D3D11Renderer::GetName() const noexcept {
  return {L"D3D11"};
}

SHM::Reader& D3D11Renderer::GetSHM() {
  return *mSHM.get();
}

void D3D11Renderer::Initialize(uint8_t /*swapchainLength*/) {
}

uint64_t D3D11Renderer::Render(
  SHM::Frame rawFrame,
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

  const auto frame = mSHM->Map(std::move(rawFrame));
  const auto sourceSRV = frame.mShaderResourceView;

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

void D3D11Renderer::SaveToDDSFile(
  SHM::Frame raw,
  const std::filesystem::path& path) {
  const auto frame = mSHM->Map(std::move(raw));

  check_hresult(
    DirectX::SaveDDSTextureToFile(
      mD3D11ImmediateContext.get(), frame.mTexture, path.wstring().c_str()));
}

}// namespace OpenKneeboard::Viewer