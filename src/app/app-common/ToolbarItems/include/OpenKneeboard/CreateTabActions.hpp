// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <memory>
#include <vector>

namespace OpenKneeboard {

class KneeboardState;
class IToolbarItem;
class TabView;
class KneeboardView;

struct InGameActions {
  std::vector<std::shared_ptr<IToolbarItem>> mLeft;
  std::vector<std::shared_ptr<IToolbarItem>> mRight;

  static InGameActions Create(
    KneeboardState* kneeboard,
    const std::shared_ptr<KneeboardView>& kneeboardView,
    const std::shared_ptr<TabView>& tab);
};

struct InAppActions {
  std::vector<std::shared_ptr<IToolbarItem>> mPrimary;
  std::vector<std::shared_ptr<IToolbarItem>> mSecondary;

  static InAppActions Create(
    KneeboardState* kneeboard,
    const std::shared_ptr<KneeboardView>& kneeboardView,
    const std::shared_ptr<TabView>& tab);
};

}// namespace OpenKneeboard
