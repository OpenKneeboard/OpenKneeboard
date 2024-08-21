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

#include <filesystem>
#include <memory>
#include <mutex>

namespace OpenKneeboard {

class DCSExtractedMission final {
 public:
  DCSExtractedMission(const DCSExtractedMission&) = delete;
  DCSExtractedMission& operator=(const DCSExtractedMission&) = delete;

  DCSExtractedMission();
  ~DCSExtractedMission() noexcept;

  static std::shared_ptr<DCSExtractedMission> Get(
    const std::filesystem::path& zipPath);

  std::filesystem::path GetZipPath() const;
  std::filesystem::path GetExtractedPath() const;

 protected:
  DCSExtractedMission(const std::filesystem::path& zipPath);

 private:
  std::filesystem::path mZipPath;
  std::filesystem::path mTempDir;

  static std::mutex sCacheMutex;
  static std::shared_ptr<DCSExtractedMission> sCache;
};

}// namespace OpenKneeboard
