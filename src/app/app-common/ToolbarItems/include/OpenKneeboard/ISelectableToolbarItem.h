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

#include <OpenKneeboard/Events.h>
#include <OpenKneeboard/IToolbarItem.h>

namespace OpenKneeboard {

struct CurrentPage_t {
  explicit CurrentPage_t() = default;
};
static constexpr CurrentPage_t CurrentPage;

struct AllPages_t {
  explicit AllPages_t() = default;
};
static constexpr AllPages_t AllPages;

struct AllTabs_t {
  explicit AllTabs_t() = default;
};
static constexpr AllTabs_t AllTabs;

class ISelectableToolbarItem : public IToolbarItem {
 public:
  virtual ~ISelectableToolbarItem() = default;

  virtual std::string_view GetGlyph() const = 0;
  virtual std::string_view GetLabel() const = 0;
  virtual bool IsEnabled() = 0;

  Event<> evStateChangedEvent;
};

}// namespace OpenKneeboard
