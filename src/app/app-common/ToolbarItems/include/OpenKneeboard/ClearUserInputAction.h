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

#include <OpenKneeboard/IToolbarItemWithConfirmation.h>
#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/inttypes.h>

namespace OpenKneeboard {

class ITabView;
class KneeboardState;
class IKneeboardView;

class ClearUserInputAction final : public ToolbarAction,
                                   public EventReceiver,
                                   public virtual IToolbarItemWithConfirmation {
 public:
  ClearUserInputAction(const std::shared_ptr<ITabView>&, CurrentPage_t);
  ClearUserInputAction(const std::shared_ptr<ITabView>&, AllPages_t);
  ClearUserInputAction(KneeboardState*, AllTabs_t);
  ClearUserInputAction() = delete;

  ~ClearUserInputAction();

  virtual bool IsEnabled() const override;
  virtual void Execute() override;

  virtual std::string_view GetConfirmationTitle() const override;
  virtual std::string_view GetConfirmationDescription() const override;
  virtual std::string_view GetConfirmButtonLabel() const override;
  virtual std::string_view GetCancelButtonLabel() const override;

 private:
  void SubscribeToTabView();

  enum class Mode { CurrentPage, ThisTab, AllTabs };
  Mode mMode;
  KneeboardState* mKneeboardState = nullptr;
  std::shared_ptr<ITabView> mTabView;
};

}// namespace OpenKneeboard
