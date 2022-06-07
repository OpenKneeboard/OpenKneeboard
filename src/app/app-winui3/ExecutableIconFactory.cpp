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

// clang-format off
#include "pch.h"
#include "ExecutableIconFactory.h"
// clang-format on

#pragma comment(lib, "Shell32.lib")

#include <wincodec.h>

using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
struct IWICImagingFactory;

namespace OpenKneeboard {

ExecutableIconFactory::ExecutableIconFactory() {
  mWIC = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory);
}

ExecutableIconFactory::~ExecutableIconFactory() {
}

winrt::com_ptr<IWICBitmap> ExecutableIconFactory::CreateWICBitmap(
  const std::filesystem::path& executable) {
  auto wpath = executable.wstring();
  HICON handle = ExtractIcon(GetModuleHandle(NULL), wpath.c_str(), 0);
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
