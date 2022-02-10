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
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/Games/DCSWorld.h>
#include <OpenKneeboard/dprint.h>
#include <Windows.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <random>

#include "okEvents.h"

using DCS = OpenKneeboard::Games::DCSWorld;

namespace OpenKneeboard {

class DCSMissionTab::ExtractedMission final {
 private:
  std::filesystem::path mTempDir;

 public:
  ExtractedMission(const ExtractedMission&) = delete;
  ExtractedMission& operator=(const ExtractedMission&) = delete;

  ExtractedMission() {
  }

  ExtractedMission(const std::filesystem::path& zip) {
    std::random_device randDevice;
    std::uniform_int_distribution<uint64_t> randDist;

    mTempDir
      = std::filesystem::path(wxStandardPaths::Get().GetTempDir().ToStdString())
      / fmt::format(
          "OpenKneeboard-{}-{:016x}-{}",
          static_cast<uint32_t>(GetCurrentProcessId()),
          randDist(randDevice),
          zip.stem().string());
    std::filesystem::create_directories(mTempDir);

    wxFileInputStream ifs(zip.wstring());
    if (!ifs.IsOk()) {
      dprintf("Can't open file {}", zip.string());
      return;
    }
    wxZipInputStream zis(ifs);
    if (!zis.IsOk()) {
      dprintf("Can't read {} as zip", zip.string());
      return;
    }

    const std::string prefix = "KNEEBOARD\\IMAGES\\";
    while (auto entry = zis.GetNextEntry()) {
      auto name = entry->GetName().ToStdString();
      if (!name.starts_with(prefix)) {
        continue;
      }
      auto path = mTempDir / name.substr(prefix.size());
      {
        wxFileOutputStream out(path.wstring());
        zis.Read(out);
      }
      if (!wxImage::CanRead(path.wstring())) {
        std::filesystem::remove(path);
      }
    }
  }

  ~ExtractedMission() {
    std::filesystem::remove_all(mTempDir);
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
}

utf8_string DCSMissionTab::GetTitle() const {
  return _("Mission");
}

void DCSMissionTab::Reload() {
  mExtracted = std::make_unique<ExtractedMission>(mMission);
  GetDelegate()->SetPath(mExtracted->GetExtractedPath());
  GetDelegate()->Reload();
}

const char* DCSMissionTab::GetGameEventName() const {
  return DCS::EVT_MISSION;
}

void DCSMissionTab::Update(
  const std::filesystem::path& _installPath,
  const std::filesystem::path& _savedGamePath,
  utf8_string_view value) {
  auto mission = std::filesystem::canonical(value);

  if (mission == mMission) {
    return;
  }
  mMission = mission;
  this->Reload();
}

}// namespace OpenKneeboard
