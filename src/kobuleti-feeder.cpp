#define _USE_MATH_DEFINES
#include <Windows.h>
#include <wincodec.h>
#include <winrt/base.h>

#include <cmath>

#include "YAVRK/SHM.h"
#include "YAVRK/ConsoleLoopCondition.h"

#pragma comment(lib, "Windowscodecs.lib")
#pragma comment(lib, "Windowsapp.lib")

static constexpr auto IMAGE_PATH
  = L"C:\\Program Files\\Eagle Dynamics\\DCS World "
    L"OpenBeta\\Mods\\terrains\\Caucasus\\Kneeboard\\01_GND_UG5X_Kobuleti_18."
    L"png";

int main() {
  CoInitialize(nullptr);
  auto imgf
    = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
  winrt::com_ptr<IWICBitmapDecoder> decoder;
  imgf->CreateDecoderFromFilename(
    IMAGE_PATH,
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnDemand,
    decoder.put());
  winrt::com_ptr<IWICBitmapFrameDecode> frame;
  decoder->GetFrame(0, frame.put());
  winrt::com_ptr<IWICFormatConverter> converter;
  imgf->CreateFormatConverter(converter.put());
  converter->Initialize(
    frame.get(),
    GUID_WICPixelFormat32bppRGBA,
    WICBitmapDitherTypeNone,
    nullptr,
    0.0,
    WICBitmapPaletteTypeMedianCut);

  UINT width, height;
  frame->GetSize(&width, &height);

  YAVRK::SHM::Header config {
    .Flags = YAVRK::Flags::DISCARD_DEPTH_INFORMATION,
    .y = 0.5f,
    .z = -0.25f,
    .rx = float(M_PI) / 2,
    .VirtualWidth = 0.3f,
    .VirtualHeight = height * 0.3f / width,
    .ImageWidth = static_cast<uint16_t>(width),
    .ImageHeight = static_cast<uint16_t>(height),
  };

  printf("Acquired SHM, feeding YAVRK - hit Ctrl-C to exit.\n");
  using Pixel = YAVRK::SHM::Pixel;
  std::vector<Pixel> pixels(config.ImageWidth * config.ImageHeight);
  YAVRK::ConsoleLoopCondition cliLoop;
  YAVRK::SHM::Writer shm;
  do {
    frame->CopyPixels(
      nullptr,
      4 * width,
      static_cast<UINT>(pixels.size() * sizeof(Pixel)),
      reinterpret_cast<BYTE*>(pixels.data()));
    shm.Update(config, pixels);
  } while (cliLoop.sleep(std::chrono::minutes(1)));
  return 0;
}
