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
#define _USE_MATH_DEFINES
#include <OpenKneeboard/ConsoleLoopCondition.h>
#include <OpenKneeboard/SHM.h>
#include <OpenKneeboard/SteamVRKneeboard.h>

#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <OpenKneeboard/tracing.h>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <DirectXTK/PostProcess.h>

#include <cmath>
#include <thread>

#include <D2D1.h>
#include <D3d11.h>
#include <d3d11_4.h>
#include <dwrite.h>
#include <dxgi1_2.h>

using namespace OpenKneeboard;

// FIXME: add test patterns tab

namespace OpenKneeboard {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("OpenKneeboard.TestFeeder")
 * 1a75505f-4493-5004-8df0-3830610c5e85
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "OpenKneeboard.TestFeeder",
  (0x1a75505f, 0x4493, 0x5004, 0x8d, 0xf0, 0x38, 0x30, 0x61, 0x0c, 0x5e, 0x85));
}// namespace OpenKneeboard

struct SharedTextureResources {
  winrt::com_ptr<ID3D11Texture2D> mTexture;
  winrt::com_ptr<ID3D11RenderTargetView> mTextureRTV;
};

int main() {
  TraceLoggingRegister(gTraceProvider);
  const scope_guard unregisterTraceProvider(
    []() { TraceLoggingUnregister(gTraceProvider); });

  DPrintSettings::Set({
    .prefix = "test-feeder",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  std::jthread OpenVRThread {[](std::stop_token stopToken) {
    SetThreadDescription(GetCurrentThread(), L"OpenVR Thread");
    SteamVRKneeboard().Run(stopToken);
  }};

  D2D1_COLOR_F colors[] = {
    {1.0f, 0.0f, 0.0f, 1.0f},// red
    {0.0f, 1.0f, 0.0f, 1.0f},// green
    {0.0f, 0.0f, 1.0f, 1.0f},// blue
    {1.0f, 0.0f, 1.0f, 0.5f},// translucent magenta
  };

  SHM::Config config;

  const uint8_t layerCount = 2;
  static_assert(layerCount <= MaxLayers);
  SHM::LayerConfig layer {
    .mImageWidth = TextureWidth,
    .mImageHeight = TextureHeight,
  };
  SHM::LayerConfig secondLayer(layer);
  secondLayer.mVR.mX = -secondLayer.mVR.mX;
  secondLayer.mVR.mRY = -secondLayer.mVR.mRY;

  uint64_t frames = -1;
  printf("Feeding OpenKneeboard - hit Ctrl-C to exit.\n");
  SHM::Writer shm;
  ConsoleLoopCondition cliLoop;

  winrt::com_ptr<ID3D11Device> device;
  UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  auto d3dLevel = D3D_FEATURE_LEVEL_11_1;
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  D3D11CreateDevice(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    d3dFlags,
    &d3dLevel,
    1,
    D3D11_SDK_VERSION,
    device.put(),
    nullptr,
    ctx.put());
  auto ctx4 = ctx.as<ID3D11DeviceContext4>();

  winrt::com_ptr<IDWriteFactory> dwrite;
  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    _uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(dwrite.put()));
  winrt::com_ptr<IDWriteTextFormat> textFormat;
  dwrite->CreateTextFormat(
    VariableWidthUIFont,
    nullptr,
    DWRITE_FONT_WEIGHT_BOLD,
    DWRITE_FONT_STYLE_NORMAL,
    DWRITE_FONT_STRETCH_NORMAL,
    36.0f,
    L"",
    textFormat.put());
  textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

  winrt::com_ptr<ID2D1Factory> d2d;
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.put());

  winrt::com_ptr<ID2D1SolidColorBrush> textBrush;

  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED);
  std::array<std::array<SharedTextureResources, TextureCount>, layerCount>
    resources;
  winrt::com_ptr<ID3D11Texture2D> canvas
    = SHM::CreateCompatibleTexture(device.get());
  winrt::com_ptr<ID2D1RenderTarget> renderTarget;
  d2d->CreateDxgiSurfaceRenderTarget(
    canvas.as<IDXGISurface>().get(),
    D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_HARDWARE,
      D2D1::PixelFormat(
        SHM::SHARED_TEXTURE_PIXEL_FORMAT, D2D1_ALPHA_MODE_PREMULTIPLIED)),
    renderTarget.put());
  renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  renderTarget->CreateSolidColorBrush(
    D2D1_COLOR_F {0.0f, 0.0f, 0.0f, 1.0f}, textBrush.put());

  // Converts pixel format if needed
  DirectX::BasicPostProcess copier(device.get());
  copier.SetEffect(DirectX::BasicPostProcess::Copy);

  for (uint8_t layerIndex = 0; layerIndex < MaxLayers; ++layerIndex) {
    auto& layerIt = resources.at(layerIndex);
    for (auto bufferIndex = 0; bufferIndex < TextureCount; ++bufferIndex) {
      auto& bufferIt = layerIt.at(bufferIndex);
      bufferIt.mTexture = SHM::CreateCompatibleTexture(
        device.get(),
        SHM::DEFAULT_D3D11_BIND_FLAGS,
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED);
      winrt::check_hresult(device->CreateRenderTargetView(
        bufferIt.mTexture.get(), nullptr, bufferIt.mTextureRTV.put()));

      HANDLE sharedHandle = INVALID_HANDLE_VALUE;
      auto textureName = shm.GetSharedTextureName(layerIndex, bufferIndex);
      dprintf(L"Creating shared handle {}", textureName);
      winrt::check_hresult(
        bufferIt.mTexture.as<IDXGIResource1>()->CreateSharedHandle(
          nullptr,
          DXGI_SHARED_RESOURCE_READ,
          textureName.c_str(),
          &sharedHandle));
    }
  }
  auto device5 = device.as<ID3D11Device5>();
  winrt::com_ptr<ID3D11Fence> fence;
  winrt::check_hresult(device5->CreateFence(
    0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(fence.put())));
  winrt::handle fenceHandle;
  winrt::check_hresult(fence->CreateSharedHandle(
    nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, fenceHandle.put()));

  D3D11_VIEWPORT viewport {
    0,
    0,
    static_cast<FLOAT>(layer.mImageWidth),
    static_cast<FLOAT>(layer.mImageHeight),
    0,
    1};
  ctx->RSSetViewports(1, &viewport);
  ID3D11ShaderResourceView* nullSRV = nullptr;
  ctx->PSSetShaderResources(0, 1, &nullSRV);

  do {
    const std::unique_lock shmLock(shm);
    const auto bufferIndex = shm.GetNextTextureIndex();
    for (uint8_t layerIndex = 0; layerIndex < MaxLayers; ++layerIndex) {
      renderTarget->BeginDraw();
      renderTarget->Clear(colors[(frames + layerIndex) % 4]);
      auto message = std::format(
        L"This Way Up\nLayer {} of {}", layerIndex + 1, MaxLayers);
      renderTarget->DrawTextW(
        message.data(),
        static_cast<UINT32>(message.length()),
        textFormat.get(),
        D2D1_RECT_F {
          0.0f,
          0.0f,
          static_cast<float>(layer.mImageWidth),
          static_cast<float>(layer.mImageHeight)},
        textBrush.get());
      winrt::check_hresult(renderTarget->EndDraw());
      renderTarget->Flush();

      winrt::com_ptr<ID3D11ShaderResourceView> srv;
      device->CreateShaderResourceView(canvas.get(), nullptr, srv.put());
      copier.SetSourceTexture(srv.get());

      auto& it = resources.at(layerIndex).at(bufferIndex);
      auto rtv = it.mTextureRTV.get();
      ctx->OMSetRenderTargets(1, &rtv, nullptr);
      copier.Process(ctx.get());
      ctx->Flush();
    }
    frames++;
    winrt::check_hresult(
      ctx4->Signal(fence.get(), shm.GetNextSequenceNumber()));

    shm.Update(config, {layer, secondLayer}, fenceHandle.get());
  } while (cliLoop.Sleep(std::chrono::seconds(1)));
  printf("Exit requested, cleaning up.\n");
  return 0;
}
