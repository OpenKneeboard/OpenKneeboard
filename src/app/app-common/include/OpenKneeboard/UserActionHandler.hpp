// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/UserAction.hpp>

#include <memory>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;
class TabView;

class UserActionHandler {
 public:
  virtual ~UserActionHandler();
  virtual bool IsEnabled() const = 0;
  [[nodiscard]]
  virtual task<void> Execute() = 0;

  static std::unique_ptr<UserActionHandler> Create(
    KneeboardState* kneeboard,
    const std::shared_ptr<KneeboardView>& kneeboardView,
    const std::shared_ptr<TabView>& tab,
    UserAction);
};

}// namespace OpenKneeboard
