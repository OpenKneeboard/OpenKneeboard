// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
// clang-format on

#include <filesystem>

using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
struct IWICImagingFactory;
struct IWICBitmap;

namespace OpenKneeboard {

class ExecutableIconFactory final {
 public:
  ExecutableIconFactory();
  ~ExecutableIconFactory();
  winrt::com_ptr<IWICBitmap> CreateWICBitmap(
    const std::filesystem::path& executable);
  BitmapSource CreateXamlBitmapSource(const std::filesystem::path& executable);

 private:
  winrt::com_ptr<IWICImagingFactory> mWIC;
};
}// namespace OpenKneeboard
