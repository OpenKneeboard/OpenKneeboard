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

#include <OpenKneeboard/IToolbarItemWithVisibility.h>
#include <OpenKneeboard/ToolbarToggleAction.h>

namespace OpenKneeboard {

class KneeboardState;
class IKneeboardView;

class ToggleBookmarkAction final : public ToolbarToggleAction,
                                   public IToolbarItemWithVisibility,
                                   private EventReceiver {
 public:
  ToggleBookmarkAction() = delete;
  ToggleBookmarkAction(
    KneeboardState*,
    const std::shared_ptr<IKneeboardView>&,
    const std::shared_ptr<ITabView>&);
  ~ToggleBookmarkAction();

  virtual bool IsVisible() const override;

  virtual bool IsActive() override;
  virtual bool IsEnabled() const override;

  virtual void Activate() override;
  virtual void Deactivate() override;

 private:
  KneeboardState* mKneeboardState = nullptr;
  std::weak_ptr<IKneeboardView> mKneeboardView;
  std::weak_ptr<ITabView> mTabView;
};

}// namespace OpenKneeboard
