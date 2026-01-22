// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

// clang-format off
#include "pch.h"
#include "ExecutableIconFactory.h"
// clang-format on

#include <shellapi.h>

#include <wincodec.h>

using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
struct IWICImagingFactory;

namespace OpenKneeboard {

ExecutableIconFactory::ExecutableIconFactory() {
  mWIC = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
}

ExecutableIconFactory::~ExecutableIconFactory() {}

winrt::com_ptr<IWICBitmap> ExecutableIconFactory::CreateWICBitmap(
  const std::filesystem::path& executable) {
  auto wpath = executable.wstring();
  HICON handle = ExtractIconW(GetModuleHandle(NULL), wpath.c_str(), 0);
  if (!handle) {
    handle = LoadIconW(NULL, IDI_APPLICATION);
  }
  winrt::com_ptr<IWICBitmap> wicBitmap;
  winrt::check_hresult(mWIC->CreateBitmapFromHICON(handle, wicBitmap.put()));
  DestroyIcon(handle);
  return wicBitmap;
}

BitmapSource ExecutableIconFactory::CreateXamlBitmapSource(
  const std::filesystem::path& executable) {
  auto wicBitmap = this->CreateWICBitmap(executable);

  UINT width, height;
  wicBitmap->GetSize(&width, &height);

  WriteableBitmap xamlBitmap(width, height);
  wicBitmap->CopyPixels(
    nullptr,
    width * 4,
    height * width * 4,
    reinterpret_cast<BYTE*>(xamlBitmap.PixelBuffer().data()));
  return xamlBitmap;
};
}// namespace OpenKneeboard
