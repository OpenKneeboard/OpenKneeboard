// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/DCSExtractedMission.hpp>
#include <OpenKneeboard/Filesystem.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <felly/guarded_data.hpp>
#include <felly/unique_any.hpp>

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

  using unique_zip_ptr = felly::unique_any<zip_t*, &zip_close>;
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
    constexpr auto RequiredFlags = ZIP_STAT_NAME | ZIP_STAT_SIZE;
    if ((zstat.valid & RequiredFlags) != RequiredFlags) {
      continue;
    }

    std::string_view name(zstat.name);
    if (name.ends_with('/')) {
      continue;
    }

    using unique_zip_file_ptr = felly::unique_any<zip_file_t*, &zip_fclose>;
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
      const auto toWrite =
        zip_fread(zipFile.get(), fileBuffer->data(), fileBuffer->size());
      if (toWrite == 0) {
        break;
      }
      if (toWrite < 0) {
        dprint.Warning(
          "zip_fread failed for {}: {}",
          zstat.name,
          zip_error_strerror(zip_file_get_error(zipFile.get())));
        file.close();
        std::filesystem::remove(filePath);
        break;
      }
      file.write(fileBuffer->data(), toWrite);
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

std::shared_ptr<DCSExtractedMission> DCSExtractedMission::Get(
  const std::filesystem::path& zipPath) {
  static felly::guarded_data<std::shared_ptr<DCSExtractedMission>> sCache;
  auto cache = sCache.lock();

  if (*cache && cache.get()->GetZipPath() == zipPath) {
    return *cache;
  }

  cache->reset(new DCSExtractedMission(zipPath));
  return *cache;
}

}// namespace OpenKneeboard
