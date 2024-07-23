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

#include <OpenKneeboard/Events.hpp>
#include <OpenKneeboard/IToolbarFlyout.hpp>
#include <OpenKneeboard/IToolbarItemWithVisibility.hpp>

#include <vector>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;

class SwitchProfileFlyout final : public EventReceiver,
                                  public virtual IToolbarFlyout,
                                  public virtual IToolbarItemWithVisibility {
 public:
  SwitchProfileFlyout(KneeboardState*);
  ~SwitchProfileFlyout();

  std::string_view GetGlyph() const override;
  std::string_view GetLabel() const override;
  virtual bool IsEnabled() const override;

  virtual bool IsVisible() const override;

  virtual std::vector<std::shared_ptr<IToolbarItem>> GetSubItems()
    const override;

  SwitchProfileFlyout() = delete;

 private:
  KneeboardState* mKneeboardState {nullptr};
};

}// namespace OpenKneeboard
