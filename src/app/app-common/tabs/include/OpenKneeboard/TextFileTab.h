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

#include <filesystem>

#include "Tab.h"
#include "TabWithDoodles.h"
#include "TabWithPlainTextContent.h"
#include "TabWithSettings.h"

namespace OpenKneeboard {

class TextFileTab final : public TabWithDoodles,
                          public TabWithPlainTextContent,
                          public TabWithSettings {
 public:
  explicit TextFileTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const std::filesystem::path& path);
  explicit TextFileTab(
    const DXResources&,
    KneeboardState*,
    utf8_string_view title,
    const nlohmann::json&);
  virtual ~TextFileTab();

  virtual utf8_string GetTitle() const override;
  virtual void Reload() override;

  virtual nlohmann::json GetSettings() const override;

  std::filesystem::path GetPath() const;
  virtual void SetPath(const std::filesystem::path& path);

 protected:
  virtual void RenderPageContent(
    ID2D1DeviceContext*,
    uint16_t pageIndex,
    const D2D1_RECT_F& rect) final override;

  virtual utf8_string GetPlaceholderText() const override;

 private:
  std::filesystem::path mPath;
};

}// namespace OpenKneeboard
