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

#include <fmt/xchar.h>

#include <string>

namespace OpenKneeboard {

void dprint(std::string_view s);
void dprint(std::wstring_view s);

template <typename... T>
void dprintf(fmt::format_string<T...> fmt, T&&... args) {
  dprint(fmt::format(fmt, std::forward<T>(args)...));
}

template <typename... T>
void dprintf(fmt::wformat_string<T...> fmt, T&&... args) {
  dprint(fmt::vformat(fmt::wstring_view(fmt), fmt::make_wformat_args(args...)));
}

struct DPrintSettings {
  enum class ConsoleOutputMode {
    DEBUG_ONLY,
    ALWAYS,
  };

  std::string prefix = "OpenKneeboard";
  ConsoleOutputMode consoleOutput = ConsoleOutputMode::DEBUG_ONLY;

  static void Set(const DPrintSettings&);
};

}// namespace OpenKneeboard
