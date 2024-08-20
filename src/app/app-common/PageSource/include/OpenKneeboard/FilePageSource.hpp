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

#include <shims/filesystem>
#include <shims/winrt/base.h>

#include <OpenKneeboard/audited_ptr.hpp>
#include <OpenKneeboard/task.hpp>

#include <memory>
#include <string>
#include <vector>

namespace OpenKneeboard {

struct DXResources;
class KneeboardState;
class IPageSource;

class FilePageSource final {
 public:
  FilePageSource() = delete;

  static std::vector<std::string> GetSupportedExtensions(
    const audited_ptr<DXResources>&) noexcept;

  static task<std::shared_ptr<IPageSource>> Create(
    audited_ptr<DXResources>,
    KneeboardState*,
    std::filesystem::path) noexcept;
};

}// namespace OpenKneeboard
