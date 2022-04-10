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
#include <OpenKneeboard/DCSMissionTab.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>
#include <Windows.h>
#include <zip.h>

#include <array>
#include <fstream>
#include <random>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

class DCSMissionTab::ExtractedMission final {
 private:
  std::filesystem::path mZipPath;
  std::filesystem::path mTempDir;

 public:
  ExtractedMission(const ExtractedMission&) = delete;
  ExtractedMission& operator=(const ExtractedMission&) = delete;

  ExtractedMission() {
  }

  ExtractedMission(const std::filesystem::path& zipPath) : mZipPath(zipPath) {
    std::random_device randDevice;
    std::uniform_int_distribution<uint64_t> randDist;

    wchar_t tempDir[MAX_PATH];
    auto tempDirLen = GetTempPathW(MAX_PATH, tempDir);

    mTempDir = std::filesystem::path(std::wstring_view(tempDir, tempDirLen))
      / fmt::format(
                 "OpenKneeboard-{}-{:016x}-{}",
                 static_cast<uint32_t>(GetCurrentProcessId()),
                 randDist(randDevice),
                 zipPath.stem().string());
    std::filesystem::create_directories(mTempDir);
    int err = 0;
    auto zipPathString = zipPath.string();
    auto zip = zip_open(zipPathString.c_str(), 0, &err);
    if (err) {
      dprintf("Failed to open zip '{}': {}", zipPathString, err);
      return;
    }
    auto closeZip = scope_guard([=] { zip_close(zip); });

    // Zip file format specifies `/` as a directory separator, not backslash,
    // and libzip respects this, even on windows.
    const std::string_view prefix {"KNEEBOARD/"};
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
      if (!name.starts_with(prefix)) {
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

  ~ExtractedMission() {
    std::filesystem::remove_all(mTempDir);
  }

  std::filesystem::path GetZipPath() const {
    return mZipPath;
  }

  std::filesystem::path GetExtractedPath() const {
    return mTempDir;
  }
};

DCSMissionTab::DCSMissionTab(const DXResources& dxr)
  : TabWithDelegate(
    std::make_shared<FolderTab>(dxr, "", std::filesystem::path {})) {
}

DCSMissionTab::~DCSMissionTab() {
  GetDelegate()->SetPath({});
}

utf8_string DCSMissionTab::GetTitle() const {
  return _("Mission");
}

void DCSMissionTab::Reload() {
  if ((!mExtracted) || mExtracted->GetZipPath() != mMission) {
    mExtracted = std::make_unique<ExtractedMission>(mMission);
  }
  auto root = mExtracted->GetExtractedPath();
  if ((!mAircraft.empty()) && std::filesystem::exists(root / "KNEEBOARD" / mAircraft / "IMAGES")) {
    GetDelegate()->SetPath(root / "KNEEBOARD" / mAircraft / "IMAGES");
  } else {
    GetDelegate()->SetPath(root / "KNEEBOARD" / "IMAGES");
  }
}

void DCSMissionTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path& _installPath,
  const std::filesystem::path& _savedGamePath) {
  if (event.name == DCS::EVT_MISSION) {
    auto mission = std::filesystem::canonical(event.value);
    if (mission == mMission) {
      return;
    }
    mMission = mission;
  } else if (event.name == DCS::EVT_AIRCRAFT) {
    if (event.value == mAircraft) {
      return;
    }
    mAircraft = event.value;
  } else {
    return;
  }

  this->Reload();
}

}// namespace OpenKneeboard
