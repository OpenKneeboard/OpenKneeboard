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

#include <shims/filesystem>

#include "ITab.h"
#include "ITabWithNavigation.h"
#include "ITabWithSettings.h"
#include "TabBase.h"
#include "TabWithDoodles.h"

namespace OpenKneeboard {

class PDFTab final : public TabBase,
                     public TabWithDoodles,
                     public ITabWithNavigation,
                     public ITabWithSettings {
 public:
  explicit PDFTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const std::filesystem::path& path);
  explicit PDFTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const nlohmann::json&);
  virtual ~PDFTab();

  virtual utf8_string GetGlyph() const override;
  virtual utf8_string GetTitle() const override;

  virtual nlohmann::json GetSettings() const override;

  virtual void Reload() final override;
  virtual uint16_t GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) final override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

  virtual bool IsNavigationAvailable() const override;
  virtual std::shared_ptr<ITab> CreateNavigationTab(
    uint16_t pageIndex) override;

  virtual void PostCursorEvent(
    EventContext ctx,
    const CursorEvent&,
    uint16_t pageIndex) override;

 protected:
  virtual void RenderPageContent(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) final override;
  virtual void RenderOverDoodles(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F&) override;

 private:
  winrt::apartment_context mUIThread;
  struct Impl;
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard
