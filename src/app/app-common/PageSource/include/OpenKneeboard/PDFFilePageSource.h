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

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/IPageSourceWithNavigation.h>

#include <shims/filesystem>
#include <shims/winrt/base.h>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;
struct DXResources;

class PDFFilePageSource final
  : virtual public IPageSourceWithCursorEvents,
    virtual public IPageSourceWithNavigation,
    public EventReceiver,
    public std::enable_shared_from_this<PDFFilePageSource> {
 private:
  explicit PDFFilePageSource(const DXResources&, KneeboardState*);

 public:
  PDFFilePageSource() = delete;
  virtual ~PDFFilePageSource();
  static std::shared_ptr<PDFFilePageSource> Create(
    const DXResources&,
    KneeboardState*,
    const std::filesystem::path& path = {});

  virtual void Reload();

  virtual PageIndex GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageIndex pageIndex) final override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

  virtual bool IsNavigationAvailable() const override;
  virtual std::vector<NavigationEntry> GetNavigationEntries() const override;

  virtual void PostCursorEvent(
    EventContext ctx,
    const CursorEvent&,
    PageIndex pageIndex) override;
  virtual bool CanClearUserInput(PageIndex) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageIndex) override;
  virtual void ClearUserInput() override;

  virtual void RenderPage(
    RenderTargetID,
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect) override;

 private:
  winrt::apartment_context mUIThread;
  struct Impl;
  std::shared_ptr<Impl> p;

  winrt::fire_and_forget ReloadRenderer();
  winrt::fire_and_forget ReloadNavigation();

  void OnFileModified(const std::filesystem::path& path);

  void RenderPageContent(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect);
  void RenderOverDoodles(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F&);
};

}// namespace OpenKneeboard
