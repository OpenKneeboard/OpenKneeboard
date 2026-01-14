// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/D3D11/Renderer.hpp>

#include <OpenKneeboard/hresult.hpp>

namespace OpenKneeboard::D3D11 {

SwapchainBufferResources::SwapchainBufferResources(
  ID3D11Device* device,
  ID3D11Texture2D* texture,
  DXGI_FORMAT renderTargetViewFormat)
  : mTexture(texture) {
  D3D11_TEXTURE2D_DESC textureDesc;
  texture->GetDesc(&textureDesc);

  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc {
    .Format = renderTargetViewFormat,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {0},
  };
  check_hresult(
    device->CreateRenderTargetView(texture, &rtvDesc, mRenderTargetView.put()));
}

Renderer::Renderer(ID3D11Device* device) {
  // `query_to` jump because we can't directly fetch an ID3D11DeviceContext4,
  // which we need for ID3D11Fence support
  wil::com_ptr<ID3D11DeviceContext> ctx;
  device->GetImmediateContext(ctx.put());
  ctx.query_to(mContext.put());

  mSpriteBatch = std::make_unique<SpriteBatch>(device);
}

void Renderer::RenderLayers(
  const SwapchainResources& sr,
  uint32_t swapchainTextureIndex,
  const SHM::D3D11::Frame& frame,
  const std::span<SHM::LayerSprite>& layers,
  RenderMode renderMode) {
  OPENKNEEBOARD_TraceLoggingScope("D3D11::Renderer::RenderLayers()");

  const auto source = frame.mShaderResourceView;

  const auto& br = sr.mBufferResources.at(swapchainTextureIndex);

  auto dest = br.mRenderTargetView.get();

  check_hresult(mContext->Wait(frame.mFence, frame.mFenceIn));
  mSpriteBatch->Begin(dest, sr.mDimensions);

  if (renderMode == RenderMode::ClearAndRender) {
    mSpriteBatch->Clear();
  }

  const auto baseTint = frame.mConfig.mTint;

  for (const auto& layer: layers) {
    const DirectX::XMVECTORF32 layerTint {
      baseTint[0] * layer.mOpacity,
      baseTint[1] * layer.mOpacity,
      baseTint[2] * layer.mOpacity,
      baseTint[3] * layer.mOpacity,
    };
    mSpriteBatch->Draw(source, layer.mSourceRect, layer.mDestRect, layerTint);
  }
  mSpriteBatch->End();
}

}// namespace OpenKneeboard::D3D11