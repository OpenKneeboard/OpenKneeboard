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
#include <OpenKneeboard/ITab.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

#include <atomic>

namespace OpenKneeboard {

static std::atomic_size_t gTabCount {0};
static const scope_guard gCheckForTabLeaks([]() {
  const auto count = gTabCount.load();
  if (count > 0) {
    dprintf("Leaking {} tabs", count);
    OPENKNEEBOARD_BREAK;
  }
});

ITab::ITab() {
  gTabCount.fetch_add(1);
}

ITab::~ITab() {
  gTabCount.fetch_sub(1);
}

}// namespace OpenKneeboard
