#include "OpenKneeboard/DCSTab.h"

#include <fmt/xchar.h>
#include <wx/msgdlg.h>

#include <algorithm>
#include <filesystem>

#include "OpenKneeboard/GameEvent.h"
#include "OpenKneeboard/Games/DCSWorld.h"
#include "OpenKneeboard/dprint.h"
#include "RuntimeFiles.h"

using DCS = OpenKneeboard::Games::DCSWorld;
using namespace OpenKneeboard;

static bool FilesDiffer(
  const std::filesystem::path& a,
  const std::filesystem::path& b) {
  const auto size = std::filesystem::file_size(a);
  if (std::filesystem::file_size(b) != size) {
    return true;
  }

  winrt::handle ah {CreateFile(
    a.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL)};
  winrt::handle bh {CreateFile(
    b.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL)};

  winrt::handle afm {
    CreateFileMappingW(ah.get(), nullptr, PAGE_READONLY, 0, 0, nullptr)};
  winrt::handle bfm {
    CreateFileMappingW(bh.get(), nullptr, PAGE_READONLY, 0, 0, nullptr)};

  auto av = reinterpret_cast<std::byte*>(
    MapViewOfFile(afm.get(), FILE_MAP_READ, 0, 0, 0));
  auto bv = reinterpret_cast<std::byte*>(
    MapViewOfFile(bfm.get(), FILE_MAP_READ, 0, 0, 0));

  auto equal = std::equal(av, av + size, bv, bv + size);

  UnmapViewOfFile(av);
  UnmapViewOfFile(bv);

  return !equal;
}

static void InstallHooks(DCS::Version version, const wxString& label) {
  const auto baseDir = DCS::GetSavedGamesPath(version);
  if (!std::filesystem::is_directory(baseDir)) {
    return;
  }

  const auto hookDir = baseDir / "Scripts" / "Hooks";
  const auto dllDest = hookDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaDest = hookDir / RuntimeFiles::DCSWORLD_HOOK_LUA;

  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);
  const auto exeDir = std::filesystem::path(buffer).parent_path();
  const auto dllSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_DLL;
  const auto luaSource = exeDir / RuntimeFiles::DCSWORLD_HOOK_LUA;

  std::wstring message;
  if (!(std::filesystem::exists(dllDest) && std::filesystem::exists(luaDest))) {
    message = fmt::format(
      _("Required hooks aren't installed for {}; would you like to install "
        "them?")
        .ToStdWstring(),
      label.ToStdWstring());
  } else if (
    FilesDiffer(dllSource, dllDest) || FilesDiffer(luaSource, luaDest)) {
    message = fmt::format(
      _("Hooks for {} are out of date; would you like to update "
        "them?")
        .ToStdWstring(),
      label.ToStdWstring());
  } else {
    // Installed and equal
    return;
  }

  wxMessageDialog dialog(
    nullptr, message, "OpenKneeboard", wxOK | wxCANCEL | wxICON_WARNING);
  if (dialog.ShowModal() != wxID_OK) {
    return;
  }

  std::filesystem::create_directories(hookDir);
  std::filesystem::copy_file(
    dllSource, dllDest, std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(
    luaSource, luaDest, std::filesystem::copy_options::overwrite_existing);
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
    std::filesystem::path installPath;
    std::filesystem::path savedGamesPath;
    std::string value;
    bool operator==(const Config&) const = default;
  };

  Config currentConfig;
  Config lastValidConfig;

  std::string lastValue;
};

DCSTab::DCSTab(const wxString& title)
  : Tab(title), p(std::make_shared<Impl>()) {
  InstallHooks();
}

DCSTab::~DCSTab() {
}

void DCSTab::OnGameEvent(const GameEvent& event) {
  if (event.name == this->GetGameEventName()) {
    p->currentConfig.value = event.value;
    Update();
    return;
  }

  if (event.name == DCS::EVT_INSTALL_PATH) {
    p->currentConfig.installPath = std::filesystem::canonical(event.value);
    Update();
    return;
  }

  if (event.name == DCS::EVT_SAVED_GAMES_PATH) {
    p->currentConfig.savedGamesPath = std::filesystem::canonical(event.value);
    Update();
    return;
  }

  if (event.name == DCS::EVT_SIMULATION_START) {
    this->OnSimulationStart();
    return;
  }
}

void DCSTab::Update() {
  auto c = p->currentConfig;
  if (c == p->lastValidConfig) {
    return;
  }

  if (c.installPath.empty()) {
    return;
  }

  if (c.savedGamesPath.empty()) {
    return;
  }

  if (c.value == p->lastValue) {
    return;
  }
  p->lastValue = c.value;

  this->Update(c.installPath, c.savedGamesPath, c.value);
}

void DCSTab::OnSimulationStart() {
}

}// namespace OpenKneeboard
