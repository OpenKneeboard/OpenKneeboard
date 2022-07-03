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
#include <OpenKneeboard/IPageSource.h>
#include <OpenKneeboard/utf8.h>
#include <d2d1_1.h>

#include <string>

namespace OpenKneeboard {

class ITab : public IPageSource {
 public:
  class RuntimeID final : public UniqueIDBase<RuntimeID> {};
  virtual ~ITab();

  virtual utf8_string GetGlyph() const = 0;
  virtual utf8_string GetTitle() const = 0;
  virtual RuntimeID GetRuntimeID() const = 0;
  virtual void Reload() = 0;

  Event<> evNeedsRepaintEvent;
  Event<> evFullyReplacedEvent;
  Event<> evAvailableFeaturesChangedEvent;
  Event<EventContext, uint16_t> evPageChangeRequestedEvent;
};

}// namespace OpenKneeboard
