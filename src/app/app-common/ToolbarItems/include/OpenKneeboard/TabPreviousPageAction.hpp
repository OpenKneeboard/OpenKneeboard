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

#include <OpenKneeboard/ToolbarAction.hpp>
#include <OpenKneeboard/UserActionHandler.hpp>

namespace OpenKneeboard {

class TabView;

class TabPreviousPageAction final : public ToolbarAction,
                                    private EventReceiver,
                                    public UserActionHandler {
 public:
  TabPreviousPageAction() = delete;

  TabPreviousPageAction(KneeboardState*, const std::shared_ptr<TabView>& state);
  ~TabPreviousPageAction();

  virtual bool IsEnabled() const override;
  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Execute() override;

 private:
  KneeboardState* mKneeboard;
  std::weak_ptr<TabView> mTabView;
};

}// namespace OpenKneeboard
