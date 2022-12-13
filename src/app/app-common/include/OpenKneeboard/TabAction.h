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
#include <OpenKneeboard/utf8.h>

namespace OpenKneeboard {

class TabAction {
 public:
  virtual ~TabAction();
  utf8_string_view GetGlyph() const;
  utf8_string_view GetLabel() const;

  virtual bool IsEnabled() = 0;
  virtual void Execute() = 0;

  Event<> evStateChangedEvent;

 protected:
  TabAction() = delete;
  TabAction(utf8_string glyph, utf8_string label);

 private:
  utf8_string mGlyph;
  utf8_string mLabel;
};

class TabToggleAction : public TabAction {
 public:
  virtual void Execute() override final;

  virtual bool IsActive() = 0;
  virtual void Activate() = 0;
  virtual void Deactivate() = 0;

 protected:
  using TabAction::TabAction;
};

}// namespace OpenKneeboard
