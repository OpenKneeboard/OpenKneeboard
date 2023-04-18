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
#include <OpenKneeboard/UserActionHandler.h>

namespace OpenKneeboard {

class ITabView;
class KneeboardState;
class IKneeboardView;

class ReloadTabAction final : public ToolbarAction,
                              public EventReceiver,
                              public UserActionHandler,
                              public virtual IToolbarItemWithConfirmation {
 public:
  ReloadTabAction(KneeboardState*, const std::shared_ptr<ITabView>&);
  ReloadTabAction(KneeboardState*, AllTabs_t);

  ~ReloadTabAction();

  virtual bool IsEnabled() const override;
  virtual void Execute() override;

  virtual std::string_view GetConfirmationTitle() const override;
  virtual std::string_view GetConfirmationDescription() const override;
  virtual std::string_view GetConfirmButtonLabel() const override;
  virtual std::string_view GetCancelButtonLabel() const override;

  ReloadTabAction() = delete;

 private:
  enum class Mode {
    AllTabs,
    ThisTab,
  };
  Mode mMode;
  KneeboardState* mKneeboardState;

  std::weak_ptr<ITabView> mTabView;
};

}// namespace OpenKneeboard
