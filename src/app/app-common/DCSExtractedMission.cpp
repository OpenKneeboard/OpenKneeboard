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

#include <OpenKneeboard/DCSExtractedMission.h>
#include <OpenKneeboard/Filesystem.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <Windows.h>
#include <zip.h>

#include <array>
#include <fstream>
#include <random>

namespace OpenKneeboard {

DCSExtractedMission::DCSExtractedMission() = default;

DCSExtractedMission::DCSExtractedMission(const std::filesystem::path& zipPath)
  : mZipPath(zipPath) {
  std::random_device randDevice;
  std::uniform_int_distribution<uint64_t> randDist;

  mTempDir = Filesystem::GetTemporaryDirectory()
    / std::format(
               "OpenKneeboard-{}-{:016x}-{}",
               static_cast<uint32_t>(GetCurrentProcessId()),
               randDist(randDevice),
               zipPath.stem().string());
  dprintf(
    L"Extracting DCS mission {} to {}", zipPath.wstring(), mTempDir.wstring());
  std::filesystem::create_directories(mTempDir);
  int err = 0;
  auto zipPathString = zipPath.string();
  auto zip = zip_open(zipPathString.c_str(), 0, &err);
  if (err) {
    dprintf("Failed to open zip '{}': {}", zipPathString, err);
    return;
  }
  auto closeZip = scope_guard([=] { zip_close(zip); });

  // 4k limit for all stack variables
  auto fileBuffer = std::make_unique<std::array<char, 1024 * 1024>>();
  for (auto i = 0; i < zip_get_num_entries(zip, 0); i++) {
    zip_stat_t zstat;
    if (zip_stat_index(zip, i, 0, &zstat) != 0) {
      continue;
    }
    std::string_view name(zstat.name);
    if (name.ends_with('/')) {
      continue;
    }

    auto zipFile = zip_fopen_index(zip, i, 0);
    if (!zipFile) {
      dprintf("Failed to open zip index {}", i);
      continue;
    }
    auto closeZipFile = scope_guard([=] { zip_fclose(zipFile); });

    const auto filePath = mTempDir / name;
    std::filesystem::create_directories(filePath.parent_path());
    std::ofstream file(filePath, std::ios::binary);

    size_t toCopy = zstat.size;
    while (toCopy > 0) {
      size_t toWrite
        = zip_fread(zipFile, fileBuffer->data(), fileBuffer->size());
      if (toWrite <= 0) {
        break;
      }
      file << std::string_view(fileBuffer->data(), toWrite);
      toCopy -= toWrite;
    }
    file.flush();
  }
}

DCSExtractedMission::~DCSExtractedMission() {
  std::filesystem::remove_all(mTempDir);
}

std::filesystem::path DCSExtractedMission::GetZipPath() const {
  return mZipPath;
}

std::filesystem::path DCSExtractedMission::GetExtractedPath() const {
  return mTempDir;
}

std::shared_ptr<DCSExtractedMission> DCSExtractedMission::sCache;
std::mutex DCSExtractedMission::sCacheMutex;

std::shared_ptr<DCSExtractedMission> DCSExtractedMission::Get(
  const std::filesystem::path& zipPath) {
  std::unique_lock lock(sCacheMutex);
  if (sCache && sCache->GetZipPath() == zipPath) {
    return sCache;
  }
  sCache = {new DCSExtractedMission(zipPath)};
  return sCache;
}

}// namespace OpenKneeboard
