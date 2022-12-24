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
#include <OpenKneeboard/ToolbarFlyout.h>

namespace OpenKneeboard {

ToolbarFlyout::ToolbarFlyout(
  std::string glyph,
  std::string label,
  const std::vector<std::shared_ptr<IToolbarItem>>& items)
  : mGlyph(glyph), mLabel(label), mSubItems(items) {
}

ToolbarFlyout::~ToolbarFlyout() = default;

bool ToolbarFlyout::IsEnabled() {
  for (auto& item: this->GetSubItems()) {
    const auto selectable
      = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      continue;
    }
    if (selectable->IsEnabled()) {
      return true;
    }
  }
  return false;
}

std::string_view ToolbarFlyout::GetGlyph() const {
  return mGlyph;
}

std::string_view ToolbarFlyout::GetLabel() const {
  return mLabel;
}

std::vector<std::shared_ptr<IToolbarItem>> ToolbarFlyout::GetSubItems() const {
  return mSubItems;
}

}// namespace OpenKneeboard
