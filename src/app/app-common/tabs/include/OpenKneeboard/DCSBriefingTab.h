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

#include "DCSTab.h"
#include "TabBase.h"
#include "TabWithDoodles.h"

namespace OpenKneeboard {

class DCSExtractedMission;
class ImagePageSource;
class PlainTextPageSource;

class DCSBriefingTab final : public TabBase,
                             public DCSTab,
                             public TabWithDoodles {
 public:
  DCSBriefingTab(const DXResources&, KneeboardState*);
  virtual ~DCSBriefingTab();
  virtual utf8_string GetGlyph() const override;
  virtual utf8_string GetTitle() const override;

  virtual uint16_t GetPageCount() const override;
  virtual D2D1_SIZE_U GetNativeContentSize(uint16_t pageIndex) override;

  virtual void Reload() noexcept override;

 protected:
  virtual void OnGameEvent(
    const GameEvent&,
    const std::filesystem::path&,
    const std::filesystem::path&) override;

  virtual void RenderPageContent(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F&) override;

 private:
  std::shared_ptr<DCSExtractedMission> mMission;
  std::unique_ptr<ImagePageSource> mImagePages;
  std::unique_ptr<PlainTextPageSource> mTextPages;
};

}// namespace OpenKneeboard
