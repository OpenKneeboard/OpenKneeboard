// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/ToolbarFlyout.hpp>

namespace OpenKneeboard {

ToolbarFlyout::ToolbarFlyout(
  std::string glyph,
  std::string label,
  const std::vector<std::shared_ptr<IToolbarItem>>& items)
  : mGlyph(glyph),
    mLabel(label),
    mSubItems(items) {
  for (const auto& item: items) {
    auto selectable = std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (selectable) {
      AddEventListener(
        selectable->evStateChangedEvent, this->evStateChangedEvent);
    }
  }
}

ToolbarFlyout::~ToolbarFlyout() { RemoveAllEventListeners(); }

bool ToolbarFlyout::IsEnabled() const {
  for (auto& item: this->GetSubItems()) {
    const auto selectable =
      std::dynamic_pointer_cast<ISelectableToolbarItem>(item);
    if (!selectable) {
      continue;
    }
    if (selectable->IsEnabled()) {
      return true;
    }
  }
  return false;
}

std::string_view ToolbarFlyout::GetGlyph() const { return mGlyph; }

std::string_view ToolbarFlyout::GetLabel() const { return mLabel; }

std::vector<std::shared_ptr<IToolbarItem>> ToolbarFlyout::GetSubItems() const {
  return mSubItems;
}

}// namespace OpenKneeboard
