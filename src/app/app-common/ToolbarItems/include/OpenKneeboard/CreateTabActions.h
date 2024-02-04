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

#include <memory>
#include <vector>

namespace OpenKneeboard {

class KneeboardState;
class IToolbarItem;
class ITabView;
class KneeboardView;

struct InGameActions {
  std::vector<std::shared_ptr<IToolbarItem>> mLeft;
  std::vector<std::shared_ptr<IToolbarItem>> mRight;

  static InGameActions Create(
    KneeboardState* kneeboard,
    const std::shared_ptr<KneeboardView>& kneeboardView,
    const std::shared_ptr<ITabView>& tab);
};

struct InAppActions {
  std::vector<std::shared_ptr<IToolbarItem>> mPrimary;
  std::vector<std::shared_ptr<IToolbarItem>> mSecondary;

  static InAppActions Create(
    KneeboardState* kneeboard,
    const std::shared_ptr<KneeboardView>& kneeboardView,
    const std::shared_ptr<ITabView>& tab);
};

}// namespace OpenKneeboard
