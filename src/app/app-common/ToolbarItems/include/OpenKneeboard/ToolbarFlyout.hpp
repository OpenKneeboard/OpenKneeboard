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

#include <OpenKneeboard/IToolbarFlyout.hpp>

#include <vector>

namespace OpenKneeboard {

class ToolbarFlyout : public IToolbarFlyout, public EventReceiver {
 public:
  ToolbarFlyout(
    std::string glyph,
    std::string label,
    const std::vector<std::shared_ptr<IToolbarItem>>& items);
  ToolbarFlyout() = delete;
  virtual ~ToolbarFlyout();

  virtual bool IsEnabled() const override;

  std::string_view GetGlyph() const override final;
  std::string_view GetLabel() const override final;
  virtual std::vector<std::shared_ptr<IToolbarItem>> GetSubItems()
    const override;

 private:
  std::string mGlyph;
  std::string mLabel;
  std::vector<std::shared_ptr<IToolbarItem>> mSubItems;
};

}// namespace OpenKneeboard
