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
#include <OpenKneeboard/ToolbarToggleAction.h>

namespace OpenKneeboard {

winrt::Windows::Foundation::IAsyncAction ToolbarToggleAction::Execute() {
  if (!this->IsEnabled()) {
    co_return;
  }

  if (this->IsActive()) {
    co_await this->Deactivate();
    co_return;
  }
  co_await this->Activate();
}

}// namespace OpenKneeboard
