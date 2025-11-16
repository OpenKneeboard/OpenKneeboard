// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/TabsList.hpp>
#include <OpenKneeboard/ToolbarAction.hpp>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;
class KneeboardView;

class NextTabAction final : public ToolbarAction, private EventReceiver {
 public:
  NextTabAction() = delete;

  NextTabAction(KneeboardState*, const std::shared_ptr<KneeboardView>&);
  ~NextTabAction();

  virtual bool IsEnabled() const override;
  [[nodiscard]]
  virtual task<void> Execute() override;

 private:
  KneeboardState* mKneeboardState;
  std::weak_ptr<KneeboardView> mKneeboardView;
};

}// namespace OpenKneeboard
