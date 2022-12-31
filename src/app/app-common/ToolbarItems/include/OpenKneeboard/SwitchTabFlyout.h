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

#include <OpenKneeboard/IToolbarFlyout.h>

#include <vector>

namespace OpenKneeboard {

class KneeboardState;
class IKneeboardView;

class SwitchTabFlyout final : public IToolbarFlyout {
 public:
  SwitchTabFlyout(KneeboardState*, const std::shared_ptr<IKneeboardView>&);
  ~SwitchTabFlyout();

  std::string_view GetGlyph() const override;
  std::string_view GetLabel() const override;
  virtual bool IsEnabled() const override;

  virtual std::vector<std::shared_ptr<IToolbarItem>> GetSubItems()
    const override;

  SwitchTabFlyout() = delete;

 private:
  KneeboardState* mKneeboardState {nullptr};
  std::shared_ptr<IKneeboardView> mKneeboardView;
};

}// namespace OpenKneeboard
