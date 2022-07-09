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

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/IPageSource.h>
#include <shims/winrt/base.h>

#include <shims/filesystem>

namespace OpenKneeboard {

class ImagePageSource final : public IPageSource {
 public:
  ImagePageSource() = delete;
  ImagePageSource(
    const DXResources&,
    const std::vector<std::filesystem::path>& paths = {});
  virtual ~ImagePageSource();

  void SetPaths(const std::vector<std::filesystem::path>&);
  std::vector<std::filesystem::path> GetPaths() const;

  virtual uint16_t GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) final override;

  bool CanOpenFile(const std::filesystem::path&) const;

  virtual void RenderPage(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) final override;

 private:
  struct Page {
    std::filesystem::path mPath;
    winrt::com_ptr<ID2D1Bitmap> mBitmap;
  };

  DXResources mDXR;
  winrt::com_ptr<IWICImagingFactory> mWIC;

  std::mutex mMutex;
  std::vector<Page> mPages = {};

  winrt::com_ptr<ID2D1Bitmap> GetPageBitmap(uint16_t index);
};

}// namespace OpenKneeboard
