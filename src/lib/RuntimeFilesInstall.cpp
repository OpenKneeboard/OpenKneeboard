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
#include <OpenKneeboard/RuntimeFiles.h>

namespace OpenKneeboard::RuntimeFiles {

void Install() {
  // Assuming we're running from build tree or program files.
  //
  // Previously, we needed to copy things out of the MSIX container.
  return;
}

void RemoveStaleFiles() noexcept {
  // As above; no copying == no stale copies
  return;
}

}// namespace OpenKneeboard::RuntimeFiles
