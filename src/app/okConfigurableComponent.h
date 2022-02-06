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

#include <shims/wx.h>

#include <nlohmann/json.hpp>

#include "Events.h"

class okConfigurableComponent : public wxEvtHandler,
                                protected OpenKneeboard::EventReceiver {
 public:
  okConfigurableComponent();
  virtual wxWindow* GetSettingsUI(wxWindow* parent) = 0;
  virtual nlohmann::json GetSettings() const = 0;

  OpenKneeboard::Event<> evSettingsChangedEvent;
};
