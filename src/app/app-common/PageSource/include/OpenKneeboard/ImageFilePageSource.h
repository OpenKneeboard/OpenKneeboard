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
#include <OpenKneeboard/IPageSourceWithNavigation.h>
#include <shims/winrt/base.h>

#include <shims/filesystem>

namespace OpenKneeboard {

class ImageFilePageSource final : public virtual IPageSource,
                                  public virtual IPageSourceWithNavigation {
 public:
  ImageFilePageSource() = delete;
  ImageFilePageSource(
    const DXResources&,
    const std::vector<std::filesystem::path>& paths = {});
  virtual ~ImageFilePageSource();

  void SetPaths(const std::vector<std::filesystem::path>&);
  std::vector<std::filesystem::path> GetPaths() const;

  virtual PageIndex GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageIndex pageIndex) final override;

  bool CanOpenFile(const std::filesystem::path&) const;
  static bool CanOpenFile(const DXResources& dxr, const std::filesystem::path&);

  virtual void RenderPage(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect) final override;

  virtual bool IsNavigationAvailable() const override;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const override;

 private:
  struct Page {
    std::filesystem::path mPath;
    winrt::com_ptr<ID2D1Bitmap> mBitmap;
  };

  DXResources mDXR;

  std::mutex mMutex;
  std::vector<Page> mPages = {};

  winrt::com_ptr<ID2D1Bitmap> GetPageBitmap(PageIndex index);
};

}// namespace OpenKneeboard
