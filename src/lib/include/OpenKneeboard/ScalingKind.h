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

namespace OpenKneeboard {

enum class ScalingKind {
  /* The source is equivalent to a 2D-array of pixels; scaling is slow and low
   * quality.
   *
   * The native content size should be **strongly** preferred.
   */
  Bitmap,
  /* The source can be rendered at any resolution with high quality, without
   * significiant performance issues.
   */
  Vector,
};

}