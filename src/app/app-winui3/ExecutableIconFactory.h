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
