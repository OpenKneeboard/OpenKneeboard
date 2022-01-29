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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#define _USE_MATH_DEFINES
#include <D2D1.h>
#include <D3d11.h>
#include <dxgi1_2.h>
#include <OpenKneeboard/ConsoleLoopCondition.h>
#include <OpenKneeboard/SHM.h>
#include <Windows.h>
#include <dwrite.h>
#include <winrt/base.h>

#include <cmath>

#pragma comment(lib, "D3d11.lib")
#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "DWrite.lib")

using namespace OpenKneeboard;

#pragma pack(push)
struct Pixel {
  uint8_t b, g, r, a;
};
#pragma pack(pop)

int main() {
  D2D1_COLOR_F colors[] = {
    {1.0f, 0.0f, 0.0f, 1.0f},// red
    {0.0f, 1.0f, 0.0f, 1.0f},// green
    {0.0f, 0.0f, 1.0f, 1.0f},// blue
    {1.0f, 0.0f, 1.0f, 0.5f},// translucent magenta
  };

  SHM::Config config {
    /* Headlocked
    .Flags = OpenKneeboard::Flags::HEADLOCKED |
    OpenKneeboard::Flags::DISCARD_DEPTH_INFORMATION, .y = -0.15, .z = -0.5f,
    .VirtualHeight = 0.25f,
    .imageWidth = 400,
    .imageHeight = 1200,
    */
    /* On knee */
    .imageWidth = 800,
    .imageHeight = 1200,
  };
  uint64_t frames = -1;
  printf("Feeding OpenKneeboard - hit Ctrl-C to exit.\n");
  std::vector<Pixel> pixels(config.imageWidth * config.imageHeight);
  SHM::Writer shm;
  ConsoleLoopCondition cliLoop;

  winrt::com_ptr<ID3D11Device> device;
  winrt::com_ptr<ID3D11DeviceContext> context;
  UINT d3dFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef DEBUG
  d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  auto d3dLevel = D3D_FEATURE_LEVEL_11_0;
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
    context.put());

  static_assert(SHM::SHARED_TEXTURE_IS_PREMULTIPLIED_B8G8R8A8);
  winrt::com_ptr<ID3D11Texture2D> texture;
  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = config.imageWidth,
    .Height = config.imageHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    .MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX,
  };
  device->CreateTexture2D(&textureDesc, nullptr, texture.put());

  winrt::com_ptr<ID3D11RenderTargetView> rt;
  device->CreateRenderTargetView(texture.get(), nullptr, rt.put());

  auto surface = texture.as<IDXGISurface>();

  winrt::com_ptr<IDWriteFactory> dwrite;
  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    _uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(dwrite.put()));
  winrt::com_ptr<IDWriteTextFormat> textFormat;
  dwrite->CreateTextFormat(
    L"Segoe UI",
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
  winrt::com_ptr<ID2D1RenderTarget> d2dRt;
  auto ret = d2d->CreateDxgiSurfaceRenderTarget(surface.get(), D2D1::RenderTargetProperties(
    D2D1_RENDER_TARGET_TYPE_HARDWARE,
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)

  ), d2dRt.put());
  d2dRt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  winrt::com_ptr<ID2D1SolidColorBrush> textBrush;
  d2dRt->CreateSolidColorBrush(D2D1_COLOR_F {0.0f, 0.0f, 0.0f, 1.0f}, textBrush.put());

  std::wstring message(L"This Way Up");

  HANDLE sharedHandle = INVALID_HANDLE_VALUE;
  auto textureName = SHM::SharedTextureName();
  texture.as<IDXGIResource1>()->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, textureName.c_str(), &sharedHandle);

  auto mutex = texture.as<IDXGIKeyedMutex>();

  UINT mutexKey = 0;
  do {
    frames++;

    mutex->AcquireSync(mutexKey, INFINITE);
    d2dRt->BeginDraw();
    d2dRt->Clear(colors[frames % 4]);
    d2dRt->DrawTextW(
      message.data(),
      static_cast<UINT32>(message.length()),
      textFormat.get(),
      D2D1_RECT_F {
        0.0f,
        0.0f,
        static_cast<float>(config.imageWidth),
        static_cast<float>(config.imageHeight)},
      textBrush.get());
    d2dRt->EndDraw();
    context->Flush();
    mutexKey = shm.GetNextTextureKey();
    mutex->ReleaseSync(mutexKey);

    shm.Update(config);
  } while (cliLoop.Sleep(std::chrono::seconds(1)));
  printf("Exit requested, cleaning up.\n");
  return 0;
}
