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
#include <OpenKneeboard/UserActionHandler.h>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;

class ToggleBookmarkAction final : public ToolbarToggleAction,
                                   public IToolbarItemWithVisibility,
                                   public UserActionHandler,
                                   private EventReceiver {
 public:
  ToggleBookmarkAction() = delete;
  ToggleBookmarkAction(
    KneeboardState*,
    const std::shared_ptr<KneeboardView>&,
    const std::shared_ptr<TabView>&);
  ~ToggleBookmarkAction();

  virtual bool IsVisible() const override;

  virtual bool IsActive() override;
  virtual bool IsEnabled() const override;

  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Activate() override;
  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Deactivate() override;
  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Execute() override;

 private:
  KneeboardState* mKneeboardState = nullptr;
  std::weak_ptr<KneeboardView> mKneeboardView;
  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
