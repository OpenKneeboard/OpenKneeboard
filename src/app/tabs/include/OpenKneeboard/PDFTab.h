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

#include <shims/wx.h>

#include <filesystem>

#include "Tab.h"
#include "TabWithDoodles.h"
#include "TabWithNavigation.h"
#include "okConfigurableComponent.h"

namespace OpenKneeboard {

class PDFTab final : public TabWithDoodles, public TabWithNavigation, public okConfigurableComponent {
 public:
  explicit PDFTab(
    const DXResources&,
    const wxString& title,
    const std::filesystem::path& path);
  explicit PDFTab(
    const DXResources&,
    const wxString& title,
    const nlohmann::json&);
  virtual ~PDFTab();

  virtual std::wstring GetTitle() const override;

  static std::shared_ptr<PDFTab> Create(wxWindow* parent, const DXResources&);
  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;

  virtual void Reload() final override;
  virtual uint16_t GetPageCount() const final override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) final override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

  virtual bool IsNavigationAvailable() const override;
  virtual std::shared_ptr<Tab> CreateNavigationTab(uint16_t pageIndex) override;

 protected:
  virtual void RenderPageContent(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) final override;

 private:
  struct Impl;
  std::shared_ptr<Impl> p;
};

}// namespace OpenKneeboard
