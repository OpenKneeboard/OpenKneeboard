// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the OpenKneeboard repository.
#pragma once

#include <OpenKneeboard/IToolbarFlyout.hpp>

#include <vector>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;

class SwitchTabFlyout final : public IToolbarFlyout, public EventReceiver {
 public:
  SwitchTabFlyout(KneeboardState*, const std::shared_ptr<KneeboardView>&);
  ~SwitchTabFlyout();

  std::string_view GetGlyph() const override;
  std::string_view GetLabel() const override;
  virtual bool IsEnabled() const override;

  virtual std::vector<std::shared_ptr<IToolbarItem>> GetSubItems()
    const override;

  SwitchTabFlyout() = delete;

 private:
  KneeboardState* mKneeboardState {nullptr};
  std::weak_ptr<KneeboardView> mKneeboardView;
};

}// namespace OpenKneeboard
