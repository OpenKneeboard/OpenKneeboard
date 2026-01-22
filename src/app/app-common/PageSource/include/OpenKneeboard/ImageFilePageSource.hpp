// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/DXResources.hpp>
#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/FilesystemWatcher.hpp>
#include <OpenKneeboard/IPageSource.hpp>
#include <OpenKneeboard/IPageSourceWithNavigation.hpp>

#include <OpenKneeboard/audited_ptr.hpp>

#include <shims/winrt/base.h>

#include <filesystem>

namespace OpenKneeboard {

class ImageFilePageSource final
  : public virtual IPageSource,
    public virtual IPageSourceWithNavigation,
    public virtual EventReceiver,
    public std::enable_shared_from_this<ImageFilePageSource> {
 public:
  static std::shared_ptr<ImageFilePageSource> Create(
    const audited_ptr<DXResources>&,
    const std::vector<std::filesystem::path>& paths = {});
  virtual ~ImageFilePageSource();

  void SetPaths(const std::vector<std::filesystem::path>&);
  std::vector<std::filesystem::path> GetPaths() const;

  virtual PageIndex GetPageCount() const final override;
  virtual std::vector<PageID> GetPageIDs() const final override;
  virtual std::optional<PreferredSize> GetPreferredSize(PageID) final override;

  bool CanOpenFile(const std::filesystem::path&) const;
  static bool CanOpenFile(
    const audited_ptr<DXResources>& dxr,
    const std::filesystem::path&);

  task<void> RenderPage(RenderContext, PageID, PixelRect rect) final override;

  virtual bool IsNavigationAvailable() const override;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const override;

  ImageFilePageSource() = delete;

  struct FileFormatProvider {
    winrt::guid mGuid;
    winrt::guid mContainerGuid;
    winrt::guid mVendorGuid;
    std::vector<std::string> mExtensions;
  };
  static std::vector<FileFormatProvider> GetFileFormatProviders(
    IWICImagingFactory*);

 private:
  ImageFilePageSource(const audited_ptr<DXResources>&);

  struct Page {
    PageID mID;
    std::filesystem::path mPath;
    winrt::com_ptr<ID2D1Bitmap> mBitmap;
    std::shared_ptr<FilesystemWatcher> mWatcher;
  };

  void OnFileModified(const std::filesystem::path&);

  audited_ptr<DXResources> mDXR;

  std::mutex mMutex;
  std::vector<Page> mPages = {};

  winrt::com_ptr<ID2D1Bitmap> GetPageBitmap(PageID);

  static winrt::com_ptr<IWICBitmapDecoder> GetDecoderFromFileName(
    IWICImagingFactory*,
    const std::filesystem::path&);
};

}// namespace OpenKneeboard
