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

namespace OpenKneeboard::Filesystem {

/** Differs from std::filesystem::temp_directory_path() in that
 * it guarantees to be in canonical form */
std::filesystem::path GetTemporaryDirectory();
std::filesystem::path GetRuntimeDirectory();

void CleanupTemporaryDirectories();

class ScopedDeleter {
 public:
  ScopedDeleter(const std::filesystem::path&);
  ~ScopedDeleter() noexcept;

  ScopedDeleter() = delete;
  ScopedDeleter(const ScopedDeleter&) = delete;
  ScopedDeleter& operator=(const ScopedDeleter&) = delete;

 private:
  std::filesystem::path mPath;
};

}// namespace OpenKneeboard::Filesystem
