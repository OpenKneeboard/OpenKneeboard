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

#include <functional>
#include <mutex>

namespace OpenKneeboard {
template <class T>
class LazyOnceValue {
 public:
  LazyOnceValue() = delete;
  LazyOnceValue(std::function<T()> f) : mFunc(f) {
  }

  operator T() noexcept {
    std::call_once(mOnce, [this]() { mValue = mFunc(); });
    return mValue;
  }

 private:
  std::function<T()> mFunc;
  std::once_flag mOnce;
  T mValue;
};

}