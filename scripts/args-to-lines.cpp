// Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: ISC

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