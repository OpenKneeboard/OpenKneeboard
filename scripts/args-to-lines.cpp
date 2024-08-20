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

// Convert command line to argv, then output one argv per line
// This is used for `make-compile-commands.ps1`, as parsing command lines
// properly - including nested quotes and spaces - is a PITA

#include <Windows.h>

#include <iostream>
#include <span>

int main() {
  int argc {0};
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (auto&& arg: std::span {argv, static_cast<size_t>(argc)}) {
    std::wcout << arg << std::endl;
  }
  return 0;
}