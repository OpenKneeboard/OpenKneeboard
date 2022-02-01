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
#include <OpenKneeboard/DrawableTab.h>

namespace OpenKneeboard {

struct DrawableTab::Impl {
};

DrawableTab::DrawableTab(const wxString& title)
  : Tab(title), p(std::make_shared<Impl>()) {
}

DrawableTab::~DrawableTab() {
}

void DrawableTab::OnCursorEvent(const CursorEvent& event) {
}

void DrawableTab::RenderPage(
  uint16_t pageIndex,
  const winrt::com_ptr<ID2D1RenderTarget>& target,
  const D2D1_RECT_F& rect) {
  this->RenderPageContent(pageIndex, target, rect);
}
}// namespace OpenKneeboard
