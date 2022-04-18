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

#include <memory>
#include <stop_token>
#include <vector>

namespace OpenKneeboard {

class OpenVROverlay final {
 private:
  class Impl;
  std::unique_ptr<Impl> p;

 public:
  OpenVROverlay();
  ~OpenVROverlay();

  bool Run(std::stop_token);

 private:
  void Tick();
};

}// namespace OpenKneeboard
