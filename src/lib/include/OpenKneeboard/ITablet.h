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

#include <OpenKneeboard/TabletInfo.h>
#include <OpenKneeboard/TabletState.h>

namespace OpenKneeboard {

class ITablet {
 public:
  virtual ~ITablet();

  virtual bool IsValid() const = 0;

  virtual TabletState GetState() const = 0;
  virtual TabletInfo GetDeviceInfo() const = 0;
};

}// namespace OpenKneeboard
