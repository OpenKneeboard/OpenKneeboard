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

namespace {
class ExtractedMission final {
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

    wxFileInputStream ifs(zip.string());
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
        wxFileOutputStream out(path.string());
        zis.Read(out);
      }
      if (!wxImage::CanRead(path.string())) {
        std::filesystem::remove(path.string());
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
}// namespace

class DCSMissionTab::Impl final {
 public:
  std::filesystem::path mMission;
  std::unique_ptr<ExtractedMission> mExtracted;
  std::unique_ptr<FolderTab> mDelegate;
};

DCSMissionTab::DCSMissionTab(const DXResources& dxr)
  : DCSTab(dxr, _("Mission")), p(std::make_shared<Impl>()) {
  p->mDelegate
    = std::make_unique<FolderTab>(dxr, wxString {}, std::filesystem::path {});
  p->mDelegate->Bind(okEVT_TAB_FULLY_REPLACED, [this](auto& ev) {
    wxQueueEvent(this, ev.Clone());
  });
}

DCSMissionTab::~DCSMissionTab() {
}

void DCSMissionTab::Reload() {
  p->mExtracted = std::make_unique<ExtractedMission>(p->mMission);
  p->mDelegate->SetPath(p->mExtracted->GetExtractedPath());
  p->mDelegate->Reload();
}

uint16_t DCSMissionTab::GetPageCount() const {
  return p->mDelegate->GetPageCount();
}

void DCSMissionTab::RenderPageContent(
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  p->mDelegate->RenderPage(pageIndex, rect);
}

D2D1_SIZE_U DCSMissionTab::GetPreferredPixelSize(uint16_t pageIndex) {
  return p->mDelegate->GetPreferredPixelSize(pageIndex);
}

const char* DCSMissionTab::GetGameEventName() const {
  return DCS::EVT_MISSION;
}

void DCSMissionTab::Update(
  const std::filesystem::path& _installPath,
  const std::filesystem::path& _savedGamePath,
  const std::string& value) {
  dprintf("Mission: {}", value);
  auto mission = std::filesystem::canonical(value);
  if (mission == p->mMission) {
    return;
  }
  p->mMission = mission;
  this->Reload();
}

}// namespace OpenKneeboard
