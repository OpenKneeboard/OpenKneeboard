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

namespace OpenKneeboard {

class FolderTab;

class DCSRadioLogTab final : public DCSTab {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  DCSRadioLogTab(const DXResources&);
  virtual ~DCSRadioLogTab();

  virtual void Reload() override;
  virtual uint16_t GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) override;

 protected:
  virtual void RenderPageContent(
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) override;

  virtual const char* GetGameEventName() const override;
  virtual void Update(
    const std::filesystem::path& installPath,
    const std::filesystem::path& savedGamesPath,
    const std::string& value) override;

  virtual void OnSimulationStart() override;
};

}// namespace OpenKneeboard
