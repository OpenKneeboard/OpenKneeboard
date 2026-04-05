// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <OpenKneeboard/DCSAircraftTab.hpp>
#include <OpenKneeboard/DCSMissionTab.hpp>
#include <OpenKneeboard/DCSRadioLogTab.hpp>
#include <OpenKneeboard/DCSTerrainTab.hpp>
#include <OpenKneeboard/Filesystem.hpp>
#include <OpenKneeboard/FolderPageSource.hpp>
#include <OpenKneeboard/KneeboardState.hpp>
#include <OpenKneeboard/PluginTab.hpp>
#include <OpenKneeboard/RuntimeFiles.hpp>
#include <OpenKneeboard/TabTypes.hpp>
#include <OpenKneeboard/TabView.hpp>
#include <OpenKneeboard/TabsList.hpp>

#include <OpenKneeboard/dprint.hpp>

#include <shims/nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>

namespace OpenKneeboard {

TabsList::TabsList(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard)
  : mDXR(dxr),
    mKneeboard(kneeboard) {}

task<std::shared_ptr<TabsList>> TabsList::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kneeboard,
  const nlohmann::json& config) {
  std::unique_ptr<TabsList> ret {new TabsList(dxr, kneeboard)};
  co_await ret->LoadSettings(config);
  co_return ret;
}

TabsList::~TabsList() { this->RemoveAllEventListeners(); }

static std::tuple<std::string, nlohmann::json> MigrateTab(
  const std::string& type,
  const nlohmann::json& settings) {
  if (type == "PDF" || type == "TextFile") {
    return {"SingleFile", settings};
  }

  return {type, settings};
}

// Polynomial hash over the first 64 KiB of a file plus its total size.
// Used to detect replaced files so stale bookmarks are not silently applied.
// Not a cryptographic hash.
static uint64_t ComputeFileHash(const std::filesystem::path& path) noexcept {
  try {
    const auto size = std::filesystem::file_size(path);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      return 0;
    }
    const auto readSize = static_cast<std::streamsize>(
      std::min<std::uintmax_t>(size, 65536));
    std::vector<char> buf(readSize);
    file.read(buf.data(), readSize);
    const auto n = static_cast<std::size_t>(file.gcount());
    uint64_t hash = size;
    for (std::size_t i = 0; i < n; ++i) {
      hash = hash * 31 + static_cast<uint8_t>(buf[i]);
    }
    return hash ? hash : 1;
  } catch (...) {
    return 0;
  }
}

task<std::shared_ptr<ITab>> TabsList::LoadTabFromJSON(
  const nlohmann::json tab) {
  if (!(tab.contains("Type") && tab.contains("Title"))) {
    co_return nullptr;
  }

  const std::string title = tab.at("Title");
  const std::string rawType = tab.at("Type");

  nlohmann::json rawSettings;
  if (tab.contains("Settings")) {
    rawSettings = tab.at("Settings");
  }

  const auto [type, settings] = MigrateTab(rawType, rawSettings);

  winrt::guid persistentID {};
  if (tab.contains("ID")) {
    persistentID = winrt::guid {tab.at("ID").get<std::string>()};
    // else handled by TabBase
  }

  std::shared_ptr<ITab> result;
  try {
#define IT(_, it) \
  if (type == #it) { \
    auto instance = co_await load_tab<it##Tab>( \
      mDXR, mKneeboard, persistentID, title, settings); \
    if (instance) { \
      result = instance; \
    } \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
    if (!result && type == "Plugin") {
      result = co_await PluginTab::Create(
        mDXR, mKneeboard, persistentID, title, settings);
    }
  } catch (const std::exception& e) {
    dprint.Error(
      "Failed to load tab {} with std::exception: {}",
      tab.value<std::string>("ID", "<no GUID>"),
      e.what());
    throw;
  } catch (const winrt::hresult_error& e) {
    dprint.Error(
      "Failed to load tab {} with HRESULT: {}",
      tab.value<std::string>("ID", "<no GUID>"),
      winrt::to_string(e.message()));
    throw;
  }

  if (!result) {
    dprint("Couldn't load tab with type {}", rawType);
    OPENKNEEBOARD_BREAK;
    co_return nullptr;
  }

  if (tab.contains("Bookmarks")) {
    if (const auto singleFile =
          std::dynamic_pointer_cast<SingleFileTab>(result)) {
      // SingleFileTab: {"FileHash": N, "Pages": [{"PageIndex": k, "Title": "..."}]}
      const auto& bm = tab.at("Bookmarks");
      if (bm.contains("FileHash") && bm.contains("Pages")) {
        const auto storedHash = bm.at("FileHash").get<uint64_t>();
        const auto& path = singleFile->GetPath();
        if (!std::filesystem::exists(path)) {
          dprint(
            "Discarding bookmarks for '{}': file not found", path.string());
        } else {
          const auto currentHash = ComputeFileHash(path);
          if (currentHash == 0 || currentHash != storedHash) {
            dprint(
              "Discarding bookmarks for '{}': file content has changed",
              path.string());
          } else {
            TabBase::PendingBookmarkData pending;
            pending.mFileHash = storedHash;
            for (const auto& entry: bm.at("Pages")) {
              if (!entry.contains("PageIndex") || !entry.contains("Title")) {
                continue;
              }
              pending.mPages.emplace_back(
                entry.at("PageIndex").get<PageIndex>(),
                entry.at("Title").get<std::string>());
            }
            if (!pending.mPages.empty()) {
              singleFile->SetPendingBookmarkRestore(std::move(pending));
            }
          }
        }
      }
    } else if (
      const auto folder = std::dynamic_pointer_cast<FolderTab>(result)) {
      // FolderTab: [{"File": "...", "FileHash": N, "LocalPageIndex": k, "Title": "..."}]
      if (!std::filesystem::is_directory(folder->GetPath())) {
        dprint(
          "Discarding bookmarks for folder '{}': directory not found",
          folder->GetPath().string());
      } else {
        std::vector<FolderTab::FolderPendingBookmark> pending;
        for (const auto& entry: tab.at("Bookmarks")) {
          if (!entry.contains("File") || !entry.contains("FileHash")
              || !entry.contains("LocalPageIndex")
              || !entry.contains("Title")) {
            continue;
          }
          const std::filesystem::path relFile {
            entry.at("File").get<std::string>()};
          const auto storedHash = entry.at("FileHash").get<uint64_t>();
          const auto absFile = folder->GetPath() / relFile;
          if (!std::filesystem::exists(absFile)) {
            dprint(
              "Skipping bookmark: file not found '{}'", absFile.string());
            continue;
          }
          const auto currentHash = ComputeFileHash(absFile);
          if (currentHash == 0 || currentHash != storedHash) {
            dprint(
              "Skipping bookmark: file content changed '{}'",
              absFile.string());
            continue;
          }
          pending.push_back({
            relFile,
            storedHash,
            entry.at("LocalPageIndex").get<PageIndex>(),
            entry.at("Title").get<std::string>(),
          });
        }
        if (!pending.empty()) {
          folder->SetFolderPendingBookmarkRestore(std::move(pending));
        }
      }
    }
  }

  co_return result;
}

task<void> TabsList::LoadSettings(nlohmann::json config) {
  if (config.is_null()) {
    co_await LoadDefaultSettings();
    co_return;
  }
  const std::vector<nlohmann::json> jsonTabs = config;

  std::vector<task<std::shared_ptr<ITab>>> awaitables;
  for (auto&& tab: jsonTabs) {
    awaitables.push_back(this->LoadTabFromJSON(tab));
  }

  decltype(mTabs) tabs;
  for (auto&& it: awaitables) {
    auto tab = co_await std::move(it);
    if (tab) {
      tabs.push_back(std::move(tab));
    }
  }

  co_await this->SetTabs(tabs);
}

task<void> TabsList::LoadDefaultSettings() {
  // This is a little awkwardly structured because MSVC 2022 17.11 crashes if I
  // put the `co_await`s in the SetTabs initializer list
  auto quickStartPending = SingleFileTab::Create(
    mDXR,
    mKneeboard,
    Filesystem::GetRuntimeDirectory() / RuntimeFiles::QUICK_START_PDF);
  auto radioLogPending = DCSRadioLogTab::Create(mDXR, mKneeboard);
  auto briefingPending = DCSBriefingTab::Create(mDXR, mKneeboard);

  auto quickStart = co_await std::move(quickStartPending);
  auto radioLog = co_await std::move(radioLogPending);
  auto briefing = co_await std::move(briefingPending);

  co_await this->SetTabs({
    quickStart,
    radioLog,
    briefing,
    std::make_shared<DCSMissionTab>(mDXR, mKneeboard),
    std::make_shared<DCSAircraftTab>(mDXR, mKneeboard),
    std::make_shared<DCSTerrainTab>(mDXR, mKneeboard),
  });
}

nlohmann::json TabsList::GetSettings() const {
  std::vector<nlohmann::json> ret;

  for (const auto& tab: mTabs) {
    std::string type;
#define IT(_, it) \
  if (type.empty() && std::dynamic_pointer_cast<it##Tab>(tab)) { \
    type = #it; \
  }
    OPENKNEEBOARD_TAB_TYPES
#undef IT
    if (type.empty() && std::dynamic_pointer_cast<PluginTab>(tab)) {
      type = "Plugin";
    }
    if (type.empty()) {
      dprint("Unknown type for tab {}", tab->GetTitle());
      OPENKNEEBOARD_BREAK;
      continue;
    }

    nlohmann::json savedTab {
      {"Type", type},
      {"Title", tab->GetTitle()},
      {"ID", winrt::to_string(winrt::to_hstring(tab->GetPersistentID()))},
    };

    auto withSettings = std::dynamic_pointer_cast<ITabWithSettings>(tab);
    if (withSettings) {
      auto settings = withSettings->GetSettings();
      if (!settings.is_null()) {
        savedTab.emplace("Settings", settings);
      }
    }

    if (const auto singleFile =
          std::dynamic_pointer_cast<SingleFileTab>(tab)) {
      // SingleFileTab: persist bookmarks guarded by a file content hash.
      nlohmann::json bmJson;
      const auto liveBookmarks = singleFile->GetBookmarks();
      if (!liveBookmarks.empty()) {
        const auto& path = singleFile->GetPath();
        if (std::filesystem::exists(path)) {
          const auto hash = ComputeFileHash(path);
          if (hash != 0) {
            nlohmann::json pages = nlohmann::json::array();
            const auto pageIDs = singleFile->GetPageIDs();
            for (const auto& bm: liveBookmarks) {
              const auto it = std::ranges::find(pageIDs, bm.mPageID);
              if (it == pageIDs.end()) {
                continue;
              }
              pages.push_back({
                {"PageIndex",
                 static_cast<PageIndex>(
                   std::distance(pageIDs.begin(), it))},
                {"Title", bm.mTitle},
              });
            }
            if (!pages.empty()) {
              bmJson = {{"FileHash", hash}, {"Pages", std::move(pages)}};
            }
          }
        }
      } else if (const auto pending = singleFile->GetPendingBookmarkData()) {
        // Pass-through during startup before content is loaded.
        if (pending->mFileHash != 0 && !pending->mPages.empty()) {
          nlohmann::json pages = nlohmann::json::array();
          for (const auto& [idx, title]: pending->mPages) {
            pages.push_back({{"PageIndex", idx}, {"Title", title}});
          }
          bmJson = {
            {"FileHash", pending->mFileHash}, {"Pages", std::move(pages)}};
        }
      }
      if (!bmJson.is_null()) {
        savedTab.emplace("Bookmarks", std::move(bmJson));
      }
    } else if (
      const auto folder = std::dynamic_pointer_cast<FolderTab>(tab)) {
      // FolderTab: persist bookmarks keyed by relative file path and hash.
      nlohmann::json bmArray = nlohmann::json::array();
      const auto liveBookmarks = folder->GetBookmarks();
      if (!liveBookmarks.empty()) {
        if (const auto* src = folder->GetPageSource()) {
          for (const auto& bm: liveBookmarks) {
            const auto info = src->GetFileAndLocalIndexForPageID(bm.mPageID);
            if (!info) {
              continue;
            }
            const auto absFile = folder->GetPath() / info->first;
            if (!std::filesystem::exists(absFile)) {
              continue;
            }
            const auto hash = ComputeFileHash(absFile);
            if (hash == 0) {
              continue;
            }
            bmArray.push_back({
              {"File", info->first.generic_string()},
              {"FileHash", hash},
              {"LocalPageIndex", info->second},
              {"Title", bm.mTitle},
            });
          }
        }
      } else {
        // Pass-through during startup before content is loaded.
        for (const auto& p: folder->GetFolderPendingBookmarkData()) {
          bmArray.push_back({
            {"File", p.mRelFile.generic_string()},
            {"FileHash", p.mFileHash},
            {"LocalPageIndex", p.mLocalPageIndex},
            {"Title", p.mTitle},
          });
        }
      }
      if (!bmArray.empty()) {
        savedTab.emplace("Bookmarks", std::move(bmArray));
      }
    }

    ret.push_back(savedTab);
    continue;
  }

  return ret;
}

std::vector<std::shared_ptr<ITab>> TabsList::GetTabs() const { return mTabs; }

task<void> TabsList::SetTabs(std::vector<std::shared_ptr<ITab>> tabs) {
  if (std::ranges::equal(tabs, mTabs)) {
    co_return;
  }

  {
    const auto oldTabs = std::exchange(mTabs, {});
    std::vector<task<void>> disposers;
    for (auto tab: oldTabs) {
      if (!std::ranges::contains(tabs, tab->GetRuntimeID(), [](auto it) {
            return it->GetRuntimeID();
          })) {
        if (auto p = std::dynamic_pointer_cast<IHasDisposeAsync>(tab)) {
          disposers.push_back(p->DisposeAsync());
        }
      }
    }
    for (auto& it: disposers) {
      co_await std::move(it);
    }
  }

  mTabs = tabs;
  for (auto token: mTabEvents) {
    RemoveEventListener(token);
  }
  mTabEvents.clear();
  for (const auto& tab: mTabs) {
    mTabEvents.push_back(AddEventListener(
      tab->evSettingsChangedEvent, this->evSettingsChangedEvent));
  }

  evTabsChangedEvent.Emit();
  evSettingsChangedEvent.Emit();
}

task<void> TabsList::InsertTab(TabIndex index, std::shared_ptr<ITab> tab) {
  auto tabs = mTabs;
  tabs.insert(tabs.begin() + index, tab);
  co_await this->SetTabs(tabs);
}

task<void> TabsList::RemoveTab(TabIndex index) {
  if (index >= mTabs.size()) {
    co_return;
  }

  auto tabs = mTabs;
  tabs.erase(tabs.begin() + index);
  co_await this->SetTabs(tabs);
}

}// namespace OpenKneeboard
