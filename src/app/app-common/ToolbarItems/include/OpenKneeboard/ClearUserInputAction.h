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

#include <OpenKneeboard/ToolbarAction.h>
#include <OpenKneeboard/inttypes.h>

namespace OpenKneeboard {

class ITab;
class KneeboardState;
class IKneeboardView;

class ClearUserInputAction final : public ToolbarAction {
 public:
  ClearUserInputAction(const std::shared_ptr<ITab>&, PageIndex);
  ClearUserInputAction(const std::shared_ptr<ITab>&, AllPages_t);
  ClearUserInputAction(KneeboardState*, AllTabs_t);
  ClearUserInputAction() = delete;

  ~ClearUserInputAction();

  virtual bool IsEnabled() override;
  virtual void Execute() override;

 private:
  enum class Mode { ThisPage, ThisTab, AllTabs };
  Mode mMode;
  KneeboardState* mKneeboardState = nullptr;
  std::shared_ptr<ITab> mTab;
  PageIndex mPageIndex;
};

}// namespace OpenKneeboard
