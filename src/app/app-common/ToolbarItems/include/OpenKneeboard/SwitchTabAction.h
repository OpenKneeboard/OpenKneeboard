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

#include <OpenKneeboard/ICheckableToolbarItem.h>
#include <OpenKneeboard/ToolbarAction.h>

namespace OpenKneeboard {

class ITab;
class KneeboardView;

class SwitchTabAction final : public ToolbarAction,
                              public EventReceiver,
                              public ICheckableToolbarItem {
 public:
  SwitchTabAction(
    const std::shared_ptr<KneeboardView>&,
    const std::shared_ptr<ITab>&);
  ~SwitchTabAction();

  virtual bool IsEnabled() const override;
  ;
  virtual bool IsChecked() const override;
  virtual void Execute() override;

  SwitchTabAction() = delete;

 private:
  std::weak_ptr<KneeboardView> mKneeboardView;
  ITab::RuntimeID mTabID;
};

}// namespace OpenKneeboard
