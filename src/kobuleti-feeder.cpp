#define _USE_MATH_DEFINES
#include <Windows.h>
#include <wincodec.h>
#include <winrt/base.h>

#include <cmath>

#include "YAVRK/SHM.h"

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

  YAVRK::SHMHeader config {
    .Version = YAVRK::IPC_VERSION,
    .Flags = YAVRK::Flags::DISCARD_DEPTH_INFORMATION,
    .y = 0.5f,
    .z = -0.25f,
    .rx = M_PI / 2,
    .VirtualWidth = 0.3f,
    .VirtualHeight = height * 0.3f / width,
    .ImageWidth = static_cast<uint16_t>(width),
    .ImageHeight = static_cast<uint16_t>(height),
  };
  auto shm = YAVRK::SHM::GetOrCreate(config);

  printf("Acquired SHM, feeding YAVRK - hit Ctrl-C to exit.\n");
  do {
    frame->CopyPixels(
      nullptr,
      4 * width,
      shm.ImageDataSize(),
      reinterpret_cast<BYTE*>(shm.ImageData()));
    Sleep(1000);
  } while (true);
  return 0;
}
