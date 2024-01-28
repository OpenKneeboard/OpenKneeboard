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
#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/FilesystemWatcher.h>
#include <OpenKneeboard/IPageSource.h>
#include <OpenKneeboard/IPageSourceWithNavigation.h>

#include <shims/filesystem>
#include <shims/winrt/base.h>

namespace OpenKneeboard {

class ImageFilePageSource final
  : public virtual IPageSource,
    public virtual IPageSourceWithNavigation,
    public virtual EventReceiver,
    public std::enable_shared_from_this<ImageFilePageSource> {
 public:
  static std::shared_ptr<ImageFilePageSource> Create(
    const std::shared_ptr<DXResources>&,
    const std::vector<std::filesystem::path>& paths = {});
  virtual ~ImageFilePageSource();

  void SetPaths(const std::vector<std::filesystem::path>&);
  std::vector<std::filesystem::path> GetPaths() const;

  virtual PageIndex GetPageCount() const final override;
  virtual std::vector<PageID> GetPageIDs() const final override;
  virtual PreferredSize GetPreferredSize(PageID) final override;

  bool CanOpenFile(const std::filesystem::path&) const;
  static bool CanOpenFile(const std::shared_ptr<DXResources>& dxr, const std::filesystem::path&);

  virtual void RenderPage(RenderTarget*, PageID, const D2D1_RECT_F& rect)
    final override;

  virtual bool IsNavigationAvailable() const override;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const override;

  ImageFilePageSource() = delete;

 private:
  ImageFilePageSource(const std::shared_ptr<DXResources>&);

  struct Page {
    PageID mID;
    std::filesystem::path mPath;
    winrt::com_ptr<ID2D1Bitmap> mBitmap;
    std::shared_ptr<FilesystemWatcher> mWatcher;
  };

  void OnFileModified(const std::filesystem::path&);

  std::shared_ptr<DXResources> mDXR;

  std::mutex mMutex;
  std::vector<Page> mPages = {};

  winrt::com_ptr<ID2D1Bitmap> GetPageBitmap(PageID);
};

}// namespace OpenKneeboard
