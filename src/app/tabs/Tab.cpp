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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <OpenKneeboard/Tab.h>

#include "okEvents.h"

namespace OpenKneeboard {

class Tab::Impl final {
 public:
  std::string title;
};

Tab::Tab(const wxString& title) : p(new Impl {.title = title.ToStdString()}) {
}

Tab::~Tab() {
}

std::string Tab::GetTitle() const {
  return p->title;
}

void Tab::Reload() {
}

void Tab::OnGameEvent(const GameEvent&) {
}

wxWindow* Tab::GetSettingsUI(wxWindow* parent) {
  return nullptr;
}

nlohmann::json Tab::GetSettings() const {
  return {};
}

}// namespace OpenKneeboard
