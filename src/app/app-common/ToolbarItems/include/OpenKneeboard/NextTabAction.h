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

#include <OpenKneeboard/TabsList.h>
#include <OpenKneeboard/ToolbarAction.h>

namespace OpenKneeboard {

class KneeboardState;
class KneeboardView;
class KneeboardView;

class NextTabAction final : public ToolbarAction, private EventReceiver {
 public:
  NextTabAction() = delete;

  NextTabAction(KneeboardState*, const std::shared_ptr<KneeboardView>&);
  ~NextTabAction();

  virtual bool IsEnabled() const override;
  [[nodiscard]]
  virtual winrt::Windows::Foundation::IAsyncAction Execute() override;

 private:
  KneeboardState* mKneeboardState;
  std::weak_ptr<KneeboardView> mKneeboardView;
};

}// namespace OpenKneeboard
