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
#include <OpenKneeboard/SHM/D3D11.h>

#include <OpenKneeboard/scope_guard.h>

namespace OpenKneeboard::SHM::D3D11 {

Texture::Texture(
  const winrt::com_ptr<ID3D11Device5>& device,
  const winrt::com_ptr<ID3D11DeviceContext4>& context)
  : mDevice(device), mContext(context) {
}

Texture::~Texture() = default;

ID3D11Texture2D* Texture::GetD3D11Texture() const noexcept {
  return mCacheTexture.get();
}

ID3D11ShaderResourceView* Texture::GetD3D11ShaderResourceView() noexcept {
  if (!mCacheShaderResourceView) {
    winrt::check_hresult(mDevice->CreateShaderResourceView(
      mCacheTexture.get(), nullptr, mCacheShaderResourceView.put()));
  }
  return mCacheShaderResourceView.get();
}

void Texture::InitializeSource(
  HANDLE textureHandle,
  HANDLE fenceHandle) noexcept {
  if (
    textureHandle == mSourceTextureHandle
    && fenceHandle == mSourceFenceHandle) {
    return;
  }

  if (textureHandle != mSourceTextureHandle) {
    mSourceTextureHandle = {};
    mSourceTexture = {};
    mCacheTexture = {};
    mCacheShaderResourceView = {};
  }

  if (fenceHandle != mSourceFenceHandle) {
    mSourceFenceHandle = {};
    mSourceFence = {};
  }

  if (!mSourceTexture) {
    winrt::check_hresult(mDevice->OpenSharedResource1(
      textureHandle, IID_PPV_ARGS(mSourceTexture.put())));

    D3D11_TEXTURE2D_DESC desc;
    mSourceTexture->GetDesc(&desc);
    winrt::check_hresult(
      mDevice->CreateTexture2D(&desc, nullptr, mCacheTexture.put()));

    mSourceTextureHandle = textureHandle;
  }

  if (!mSourceFence) {
    winrt::check_hresult(
      mDevice->OpenSharedFence(fenceHandle, IID_PPV_ARGS(mSourceFence.put())));
  }
}

void Texture::CopyFrom(
  HANDLE sourceTexture,
  HANDLE sourceFence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScopedActivity(
    activity, "SHM::D3D11::Texture::CopyFrom");
  this->InitializeSource(sourceTexture, sourceFence);
  TraceLoggingWriteTagged(
    activity, "FenceIn", TraceLoggingValue(fenceValueIn, "Value"));
  winrt::check_hresult(mContext->Wait(mSourceFence.get(), fenceValueIn));
  mContext->CopySubresourceRegion(
    mCacheTexture.get(), 0, 0, 0, 0, mSourceTexture.get(), 0, nullptr);
  TraceLoggingWriteTagged(
    activity, "FenceOut", TraceLoggingValue(fenceValueOut, "Value"));
  winrt::check_hresult(mContext->Signal(mSourceFence.get(), fenceValueOut));
}

CachedReader::CachedReader(
  ID3D11Device* device,
  ConsumerKind consumerKind,
  uint8_t swapchainLength)
  : SHM::CachedReader(this, consumerKind, swapchainLength) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::CachedReader()");
  winrt::check_hresult(device->QueryInterface(mD3D11Device.put()));
  winrt::com_ptr<ID3D11DeviceContext> context;
  device->GetImmediateContext(context.put());
  mD3D11DeviceContext = context.as<ID3D11DeviceContext4>();

  // Everything below here is unnecessary, but adds useful debug
  // logging.
  auto dxgiDevice = mD3D11Device.as<IDXGIDevice>();
  winrt::com_ptr<IDXGIAdapter> dxgiAdapter;
  winrt::check_hresult(dxgiDevice->GetAdapter(dxgiAdapter.put()));
  DXGI_ADAPTER_DESC desc {};
  winrt::check_hresult(dxgiAdapter->GetDesc(&desc));
  dprintf(
    L"SHM reader using adapter '{}' (LUID {:#x})",
    desc.Description,
    std::bit_cast<uint64_t>(desc.AdapterLuid));
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::~CachedReader()");
}

void CachedReader::Copy(
  HANDLE sourceTexture,
  IPCClientTexture* destinationTexture,
  HANDLE fence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D11::CachedReader::Copy()");
  reinterpret_cast<SHM::D3D11::Texture*>(destinationTexture)
    ->CopyFrom(sourceTexture, fence, fenceValueIn, fenceValueOut);
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  [[maybe_unused]] uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D11::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<SHM::D3D11::Texture>(
    mD3D11Device, mD3D11DeviceContext);
}

namespace Renderer {
DeviceResources::DeviceResources(ID3D11Device* device) {
  mD3D11Device.copy_from(device);
  device->GetImmediateContext(mD3D11ImmediateContext.put());
  mDXTKSpriteBatch
    = std::make_unique<DirectX::SpriteBatch>(mD3D11ImmediateContext.get());
}

SwapchainResources::SwapchainResources(
  DeviceResources* dr,
  DXGI_FORMAT renderTargetViewFormat,
  size_t textureCount,
  ID3D11Texture2D** textures) {
  D3D11_RENDER_TARGET_VIEW_DESC rtvDesc {
    .Format = renderTargetViewFormat,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
    .Texture2D = {.MipSlice = 0},
  };

  mBufferResources.reserve(textureCount);

  for (size_t i = 0; i < textureCount; ++i) {
    auto br = std::make_unique<BufferResources>();

    auto texture = textures[i];
    br->m3D11Texture.copy_from(texture);
    winrt::check_hresult(dr->mD3D11Device->CreateRenderTargetView(
      texture, &rtvDesc, br->mD3D11RenderTargetView.put()));

    mBufferResources.push_back(std::move(br));
  }

  D3D11_TEXTURE2D_DESC colorDesc {};
  textures[0]->GetDesc(&colorDesc);

  mViewport = {
    0,
    0,
    static_cast<FLOAT>(colorDesc.Width),
    static_cast<FLOAT>(colorDesc.Height),
    0.0f,
    1.0f,
  };
}

void BeginFrame(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "OpenKneeboard::SHM::D3D11::Renderer::BeginFrame()");

  auto& br = sr->mBufferResources.at(swapchainTextureIndex);

  auto ctx = dr->mD3D11ImmediateContext.get();
  auto rtv = br->mD3D11RenderTargetView.get();

  ctx->RSSetViewports(1, &sr->mViewport);
  ctx->OMSetDepthStencilState(nullptr, 0);
  ctx->OMSetRenderTargets(1, &rtv, nullptr);
  ctx->OMSetBlendState(nullptr, nullptr, ~static_cast<UINT>(0));
  ctx->IASetInputLayout(nullptr);
  ctx->VSSetShader(nullptr, nullptr, 0);

  TraceLoggingWriteStop(
    activity, "OpenKneeboard::SHM::D3D11::Renderer::BeginFrame()");
}

void ClearRenderTargetView(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex) {
  auto rtv = sr->mBufferResources.at(swapchainTextureIndex)
               ->mD3D11RenderTargetView.get();
  dr->mD3D11ImmediateContext->ClearRenderTargetView(
    rtv, DirectX::Colors::Transparent);
}

void Render(
  DeviceResources* dr,
  SwapchainResources* sr,
  uint8_t swapchainTextureIndex,
  const SHM::D3D11::CachedReader& shm,
  const SHM::Snapshot& snapshot,
  size_t layerSpriteCount,
  LayerSprite* layerSprites) {
  TraceLoggingThreadActivity<gTraceProvider> activity;
  TraceLoggingWriteStart(
    activity, "OpenKneeboard::SHM::D3D11::Renderer::Render()");

  auto ctx = dr->mD3D11ImmediateContext.get();
  auto sprites = dr->mDXTKSpriteBatch.get();

  {
    TraceLoggingThreadActivity<gTraceProvider> spritesActivity;
    TraceLoggingWriteStart(spritesActivity, "SpriteBatch");
    sprites->Begin();
    const scope_guard endSprites([&sprites, &spritesActivity]() {
      sprites->End();
      TraceLoggingWriteStop(spritesActivity, "SpriteBatch");
    });

    for (size_t i = 0; i < layerSpriteCount; ++i) {
      const auto& sprite = layerSprites[i];
      auto resources = snapshot.GetLayerGPUResources<SHM::D3D11::Texture>(
        sprite.mLayerIndex);

      const auto srv = resources->GetD3D11ShaderResourceView();

      if (!srv) {
        dprint("Failed to get shader resource view");
        TraceLoggingWriteStop(
          activity,
          "OpenKneeboard::SHM::D3D11::Renderer::Render()",
          TraceLoggingValue(false, "Success"));
        return;
      }

      auto config = snapshot.GetLayerConfig(sprite.mLayerIndex);

      const D3D11_RECT sourceRect = config->mLocationOnTexture;
      const D3D11_RECT destRect = sprite.mDestRect;

      const auto opacity = sprite.mOpacity;
      DirectX::FXMVECTOR tint {opacity, opacity, opacity, opacity};

      sprites->Draw(srv, destRect, &sourceRect, tint);
    }
  }

  TraceLoggingWriteStop(
    activity,
    "OpenKneeboard::SHM::D3D11::Renderer::Render()",
    TraceLoggingValue(true, "Success"));
}

void EndFrame(
  DeviceResources*,
  SwapchainResources*,
  [[maybe_unused]] uint8_t swapchainTextureIndex) {
}

}// namespace Renderer

}// namespace OpenKneeboard::SHM::D3D11