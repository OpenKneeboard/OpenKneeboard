/*
 * OpenKneeboard
 *
 * Copyright (C) 2025 Fred Emmott <fred@fredemmott.com>
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

namespace OpenKneeboard {

class ChromiumApp final {
 public:
  ChromiumApp();
  ~ChromiumApp();

 private:
  // I usually dislike PImpl, but in this case, I don't want to expose
  // generically-named macros to things using this class, such as
  // IMPLEMENT_REFCOUNTING and DISALLOW_COPY_AND_ASSIGN
  struct Wrapper;
  class Impl;
  std::unique_ptr<Wrapper> p;
};

}// namespace OpenKneeboard