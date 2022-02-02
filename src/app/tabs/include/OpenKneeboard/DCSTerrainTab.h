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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include <OpenKneeboard/DCSTab.h>
#include <OpenKneeboard/DrawableTab.h>

namespace OpenKneeboard {

struct DXResources;
class FolderTab;

class DCSTerrainTab final : public DCSTab, public DrawableTab {
 public:
  DCSTerrainTab(const DXResources&);
  virtual ~DCSTerrainTab();

  virtual void Reload() override;
  virtual uint16_t GetPageCount() const override;

 protected:
  virtual void RenderPageContent(
    uint16_t pageIndex,
    const winrt::com_ptr<ID2D1RenderTarget>& target,
    const D2D1_RECT_F& rect) final override;
  virtual D2D1_SIZE_U GetPreferredPixelSize(uint16_t pageIndex) override;

  virtual const char* GetGameEventName() const override;
  virtual void Update(
    const std::filesystem::path&,
    const std::filesystem::path&,
    const std::string&) override;

 private:
  std::shared_ptr<FolderTab> mDelegate;
};

}// namespace OpenKneeboard
