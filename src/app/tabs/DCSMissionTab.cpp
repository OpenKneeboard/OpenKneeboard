#include "OpenKneeboard/DCSMissionTab.h"

#include <Windows.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <random>

#include "OpenKneeboard/FolderTab.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"
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
      dprintf("Can't read {} as zip");
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
  std::filesystem::path Mission;
  std::unique_ptr<ExtractedMission> Extracted;
  std::unique_ptr<FolderTab> Delegate;
};

DCSMissionTab::DCSMissionTab()
  : DCSTab(_("Mission")), p(std::make_shared<Impl>()) {
  p->Delegate = std::make_unique<FolderTab>("", "");
  p->Delegate->Bind(okEVT_TAB_FULLY_REPLACED, [this](auto& ev) {
    wxQueueEvent(this, ev.Clone());
  });
}

DCSMissionTab::~DCSMissionTab() {
}

void DCSMissionTab::Reload() {
  p->Extracted = std::make_unique<ExtractedMission>(p->Mission);
  p->Delegate->SetPath(p->Extracted->GetExtractedPath());
  p->Delegate->Reload();
}

uint16_t DCSMissionTab::GetPageCount() const {
  return p->Delegate->GetPageCount();
}

wxImage DCSMissionTab::RenderPage(uint16_t index) {
  return p->Delegate->RenderPage(index);
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
  if (mission == p->Mission) {
    return;
  }
  p->Mission = mission;
  this->Reload();
}

}// namespace OpenKneeboard
