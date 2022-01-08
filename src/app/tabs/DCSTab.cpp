#include "OpenKneeboard/DCSTab.h"

#include <filesystem>

#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"
#include "RuntimeFiles.h"

#include <fmt/xchar.h>
#include <wx/msgdlg.h>

using DCS = OpenKneeboard::Games::DCSWorld;
using namespace OpenKneeboard;

static void InstallHooks(DCS::Version version, const wxString& label) {
  const auto baseDir = DCS::GetSavedGamesPath(version);
  if (!std::filesystem::is_directory(baseDir)) {
    return;
  }

  const auto hookDir = baseDir / "Scripts" / "Hooks";
  const auto dllDest = hookDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaDest = hookDir / RuntimeFiles::DCSWORLD_HOOK_LUA;

  if (std::filesystem::exists(dllDest) && std::filesystem::exists(luaDest)) {
    return;
  }

  const auto message = fmt::format(
    _("Required hooks aren't installed for {}; would you like to install them?").ToStdWstring(),
    label.ToStdWstring());
  
  wxMessageDialog dialog(nullptr, message, "OpenKneeboard", wxOK | wxCANCEL | wxICON_WARNING);
  if (dialog.ShowModal() != wxID_OK) {
    return;
  }

  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);
  const auto exeDir = std::filesystem::path(buffer).parent_path();
  const auto dllSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_LUA;

  std::filesystem::create_directories(hookDir);
  std::filesystem::copy_file(dllSource, dllDest);
  std::filesystem::copy_file(luaSource, luaDest);
}

static void InstallHooks() {
  static bool sFirstRun = true;
  if (!sFirstRun) {
    return;
  }
  sFirstRun = false;

  InstallHooks(DCS::Version::OPEN_BETA, _("DCS World (Open Beta)"));
  InstallHooks(DCS::Version::STABLE, _("DCS World (Stable)"));
}

namespace OpenKneeboard {

class DCSTab::Impl final {
 public:
  struct Config final {
    std::filesystem::path InstallPath;
    std::filesystem::path SavedGamesPath;
    std::string Value;
    bool operator==(const Config&) const = default;
  };

  Config CurrentConfig;
  Config LastValidConfig;

  std::string LastValue;
};

DCSTab::DCSTab(const wxString& title)
  : Tab(title), p(std::make_shared<Impl>()) {
  InstallHooks();
}

DCSTab::~DCSTab() {
}

void DCSTab::OnGameEvent(const GameEvent& event) {
  if (event.Name == this->GetGameEventName()) {
    p->CurrentConfig.Value = event.Value;
    Update();
    return;
  }

  if (event.Name == DCS::EVT_INSTALL_PATH) {
    p->CurrentConfig.InstallPath = std::filesystem::canonical(event.Value);
    Update();
    return;
  }

  if (event.Name == DCS::EVT_SAVED_GAMES_PATH) {
    p->CurrentConfig.SavedGamesPath = std::filesystem::canonical(event.Value);
    Update();
    return;
  }

  if (event.Name == DCS::EVT_SIMULATION_START) {
    this->OnSimulationStart();
    return;
  }
}

void DCSTab::Update() {
  auto c = p->CurrentConfig;
  if (c == p->LastValidConfig) {
    return;
  }

  if (c.InstallPath.empty()) {
    return;
  }

  if (c.SavedGamesPath.empty()) {
    return;
  }

  if (c.Value == p->LastValue) {
    return;
  }
  p->LastValue = c.Value;

  this->Update(c.InstallPath, c.SavedGamesPath, c.Value);
}

void DCSTab::OnSimulationStart() {
}

}// namespace OpenKneeboard
