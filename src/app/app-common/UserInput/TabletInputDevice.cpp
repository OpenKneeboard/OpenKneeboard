// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/TabletInputDevice.hpp>
#include <OpenKneeboard/UserInputButtonBinding.hpp>

#include <OpenKneeboard/utf8.hpp>

#include <format>

namespace OpenKneeboard {

TabletInputDevice::TabletInputDevice(
  const std::string& name,
  const std::string& id,
  TabletOrientation orientation)
  : mName(name),
    mID(id),
    mOrientation(orientation) {}

TabletInputDevice::~TabletInputDevice() {}

std::string TabletInputDevice::GetName() const { return mName; }

std::string TabletInputDevice::GetID() const { return mID; }

std::string TabletInputDevice::GetButtonComboDescription(
  const std::unordered_set<uint64_t>& ids) const {
  if (ids.empty()) {
    return _("None");
  }
  if (ids.size() == 1) {
    return std::format(_("Key {}"), *ids.begin() + 1);
  }
  std::string out;
  for (auto id: ids) {
    if (!out.empty()) {
      out += " + ";
    }
    out += std::to_string(id + 1);
  }
  return out;
}

std::vector<UserInputButtonBinding> TabletInputDevice::GetButtonBindings()
  const {
  return mButtonBindings;
}

void TabletInputDevice::SetButtonBindings(
  const std::vector<UserInputButtonBinding>& bindings) {
  mButtonBindings = bindings;
  evBindingsChangedEvent.Emit();
}

TabletOrientation TabletInputDevice::GetOrientation() const {
  return mOrientation;
}

void TabletInputDevice::SetOrientation(TabletOrientation value) {
  mOrientation = value;
  evOrientationChangedEvent.Emit(value);
}

}// namespace OpenKneeboard
