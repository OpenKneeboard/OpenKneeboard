#define _USE_MATH_DEFINES
#include <D2D1.h>
#include <D3d11.h>
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

int main() {
  D2D1_COLOR_F colors[] = {
    {1.0f, 0.0f, 0.0f, 1.0f},// red
    {0.0f, 1.0f, 0.0f, 1.0f},// green
    {0.0f, 0.0f, 1.0f, 1.0f},// blue
    {1.0f, 0.0f, 1.0f, 0.5f},// translucent magenta
  };
  using Pixel = OpenKneeboard::SHM::Pixel;
  static_assert(Pixel::IS_PREMULTIPLIED_B8G8R8A8);

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
    .vr = {.flags = SHM::VRConfig::Flags::DISCARD_DEPTH_INFORMATION},
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

  winrt::com_ptr<ID3D11Texture2D> texture, mappableTexture;
  D3D11_TEXTURE2D_DESC textureDesc {
    .Width = config.imageWidth,
    .Height = config.imageHeight,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
    .SampleDesc = {1, 0},
    .BindFlags = D3D11_BIND_RENDER_TARGET,
  };
  device->CreateTexture2D(&textureDesc, nullptr, texture.put());
  textureDesc.BindFlags = 0;
  textureDesc.Usage = D3D11_USAGE_STAGING;
  textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  device->CreateTexture2D(&textureDesc, nullptr, mappableTexture.put());

  winrt::com_ptr<ID3D11RenderTargetView> rt;
  device->CreateRenderTargetView(texture.get(), nullptr, rt.put());

  auto surface = texture.as<IDXGISurface>();
  auto mappableSurface = mappableTexture.as<IDXGISurface>();

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
  winrt::com_ptr<ID2D1SolidColorBrush> textBrush;
  d2dRt->CreateSolidColorBrush(D2D1_COLOR_F {0.0f, 0.0f, 0.0f, 1.0f}, textBrush.put());

  std::wstring message(L"This Way Up");

  do {
    frames++;

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
    context->CopyResource(mappableTexture.get(), texture.get());

    DXGI_MAPPED_RECT mapped;
    mappableSurface->Map(&mapped, DXGI_MAP_READ);
    memcpy(pixels.data(), mapped.pBits, sizeof(SHM::Pixel) * pixels.size());
    mappableSurface->Unmap();

    shm.Update(config, pixels);
  } while (cliLoop.Sleep(std::chrono::seconds(1)));
  printf("Exit requested, cleaning up.\n");
  return 0;
}
