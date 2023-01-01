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
#include <OpenKneeboard/IPageSourceWithCursorEvents.h>
#include <OpenKneeboard/ITabWithSettings.h>
#include <OpenKneeboard/TabBase.h>

#include <shims/filesystem>

#include "ITab.h"
#include "ITabWithSettings.h"
#include "TabBase.h"

namespace OpenKneeboard {

class DoodleRenderer;
class KneeboardState;

class EndlessNotebookTab final : public TabBase,
                                 public virtual ITabWithSettings,
                                 public virtual IPageSourceWithCursorEvents,
                                 public EventReceiver {
 public:
  EndlessNotebookTab(
    const DXResources&,
    KneeboardState*,
    const std::filesystem::path& path);
  EndlessNotebookTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const nlohmann::json&);
  virtual ~EndlessNotebookTab();

  virtual std::string GetGlyph() const override;
  static std::string GetStaticGlyph();

  virtual void Reload() override;

  virtual nlohmann::json GetSettings() const override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

  virtual PageIndex GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(PageIndex pageIndex) override;
  virtual void RenderPage(
    ID2D1DeviceContext*,
    PageIndex pageIndex,
    const D2D1_RECT_F& rect) override;

  virtual void PostCursorEvent(
    EventContext,
    const CursorEvent&,
    PageIndex pageIndex) override;
  virtual bool CanClearUserInput(PageIndex) const override;
  virtual bool CanClearUserInput() const override;
  virtual void ClearUserInput(PageIndex) override;
  virtual void ClearUserInput() override;

 private:
  EndlessNotebookTab(
    const DXResources&,
    KneeboardState*,
    const winrt::guid& persistentID,
    std::string_view title,
    const std::filesystem::path& path);
  DXResources mDXR;
  KneeboardState* mKneeboard;

  std::filesystem::path mPath;

  std::shared_ptr<IPageSource> mSource;
  std::unique_ptr<DoodleRenderer> mDoodles;
  PageIndex mPageCount {0};
};

}// namespace OpenKneeboard