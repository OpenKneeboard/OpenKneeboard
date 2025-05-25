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

#include <OpenKneeboard/DCSExtractedMission.hpp>
#include <OpenKneeboard/Filesystem.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/handles.hpp>

#include <Windows.h>

#include <array>
#include <fstream>
#include <random>

#include <zip.h>

namespace OpenKneeboard {

DCSExtractedMission::DCSExtractedMission() = default;

DCSExtractedMission::DCSExtractedMission(const std::filesystem::path& zipPath)
  : mZipPath(zipPath) {
  std::random_device randDevice;
  std::uniform_int_distribution<uint64_t> randDist;

  mTempDir = Filesystem::GetTemporaryDirectory()
    / std::format("{:016x}", randDist(randDevice));
  dprint(
    L"Extracting DCS mission {} to {}", zipPath.wstring(), mTempDir.wstring());
  std::filesystem::create_directories(mTempDir);
  int err = 0;
  auto zipPathString = zipPath.string();

  using unique_zip_ptr = std::unique_ptr<zip_t, CPtrDeleter<zip_t, &zip_close>>;
  unique_zip_ptr zip {zip_open(zipPathString.c_str(), ZIP_RDONLY, &err)};
  if (err) {
    dprint("Failed to open zip '{}': {}", zipPathString, err);
    return;
  }

  // Use the heap because there's 4k limit for all stack variables
  auto fileBuffer = std::make_unique<std::array<char, 1024 * 1024>>();
  for (auto i = 0; i < zip_get_num_entries(zip.get(), 0); i++) {
    zip_stat_t zstat;
    if (zip_stat_index(zip.get(), i, 0, &zstat) != 0) {
      continue;
    }
    std::string_view name(zstat.name);
    if (name.ends_with('/')) {
      continue;
    }

    using unique_zip_file_ptr
      = std::unique_ptr<zip_file_t, CPtrDeleter<zip_file_t, &zip_fclose>>;
    unique_zip_file_ptr zipFile {zip_fopen_index(zip.get(), i, 0)};
    if (!zipFile) {
      dprint("Failed to open zip index {}", i);
      continue;
    }

    const auto filePath = mTempDir / name;
    std::filesystem::create_directories(filePath.parent_path());
    std::ofstream file(filePath, std::ios::binary);

    size_t toCopy = zstat.size;
    while (toCopy > 0) {
      size_t toWrite
        = zip_fread(zipFile.get(), fileBuffer->data(), fileBuffer->size());
      if (toWrite <= 0) {
        break;
      }
      file << std::string_view(fileBuffer->data(), toWrite);
      toCopy -= toWrite;
    }
    file.flush();
  }
}

DCSExtractedMission::~DCSExtractedMission() noexcept {
  std::error_code ec;
  std::filesystem::remove_all(mTempDir, ec);
  if (ec) {
    // Expected if e.g. antivirus is looking at the folder
    dprint(
      "Error removing extracted mission directory: {} ({})",
      ec.message(),
      ec.value());
  }
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
  sCache.reset(new DCSExtractedMission(zipPath));
  return sCache;
}

}// namespace OpenKneeboard
