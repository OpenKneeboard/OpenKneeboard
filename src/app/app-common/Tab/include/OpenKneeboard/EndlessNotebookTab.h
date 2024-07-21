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

#include "ITab.h"
#include "ITabWithSettings.h"
#include "TabBase.h"

#include <OpenKneeboard/DXResources.h>
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/ITabWithSettings.h>
#include <OpenKneeboard/TabBase.h>

#include <OpenKneeboard/audited_ptr.h>

#include <shims/filesystem>

namespace OpenKneeboard {

class DoodleRenderer;
class KneeboardState;

class EndlessNotebookTab final
  : public TabBase,
    public virtual ITabWithSettings,
    public virtual IPageSourceWithCursorEvents,
    public virtual EventReceiver,
    public std::enable_shared_from_this<EndlessNotebookTab> {
 public:
  static std::shared_ptr<EndlessNotebookTab> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const std::filesystem::path& path);
  static std::shared_ptr<EndlessNotebookTab> Create(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json&);
  virtual ~EndlessNotebookTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Reload() override;

  virtual nlohmann::json GetSettings() const override;

  std::filesystem::path GetPath() const;

  [[nodiscard]] virtual winrt::Windows::Foundation::IAsyncAction SetPath(
    std::filesystem::path path);

  virtual PageIndex GetPageCount() const override;
  virtual std::vector<PageID> GetPageIDs() const override;
  virtual PreferredSize GetPreferredSize(PageID) override;
  virtual void RenderPage(const RenderContext&, PageID, const PixelRect& rect)
    override;

  virtual void PostCursorEvent(EventContext, const CursorEvent&, PageID)
    override;
  virtual bool CanClearUserInput(PageID) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageID) override;
  virtual void ClearUserInput() override;

  EndlessNotebookTab() = delete;

 private:
  EndlessNotebookTab(
    const audited_ptr<DXResources>&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title);
  audited_ptr<DXResources> mDXR;
  KneeboardState* mKneeboard;

  std::filesystem::path mPath;
  void SetPathFromEmpty(const std::filesystem::path& path);

  std::shared_ptr<IPageSource> mSource;
  PageID mSourcePageID;
  std::unique_ptr<DoodleRenderer> mDoodles;
  std::vector<PageID> mPageIDs;

  void OnSourceContentChanged();
};

}// namespace OpenKneeboard
