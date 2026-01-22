// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <OpenKneeboard/APIEvent.hpp>
#include <OpenKneeboard/DCSBriefingTab.hpp>
#include <OpenKneeboard/DCSEvents.hpp>
#include <OpenKneeboard/DCSExtractedMission.hpp>
#include <OpenKneeboard/ImageFilePageSource.hpp>
#include <OpenKneeboard/Lua.hpp>
#include <OpenKneeboard/NavigationTab.hpp>
#include <OpenKneeboard/PlainTextPageSource.hpp>

#include <OpenKneeboard/dprint.hpp>
#include <OpenKneeboard/format/filesystem.hpp>
#include <OpenKneeboard/scope_exit.hpp>

namespace OpenKneeboard {
task<std::shared_ptr<DCSBriefingTab>> DCSBriefingTab::Create(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs) {
  return Create(dxr, kbs, {}, _("Briefing"));
}

DCSBriefingTab::DCSBriefingTab(
  const audited_ptr<DXResources>& dxr,
  KneeboardState* kbs,
  const winrt::guid& persistentID,
  std::string_view title)
  : DCSTab(kbs),
    PageSourceWithDelegates(dxr, kbs),
    TabBase(persistentID, title),
    mKneeboard(kbs),
    mImagePages(ImageFilePageSource::Create(dxr)),
    mTextPages(
      std::make_unique<PlainTextPageSource>(dxr, kbs, _("[no briefing]"))) {}

task<std::shared_ptr<DCSBriefingTab>> DCSBriefingTab::Create(
  audited_ptr<DXResources> dxr,
  KneeboardState* kbs,
  winrt::guid persistentID,
  std::string_view title) {
  std::shared_ptr<DCSBriefingTab> ret {
    new DCSBriefingTab(dxr, kbs, persistentID, title)};
  co_await ret->SetDelegates({
    std::static_pointer_cast<IPageSource>(ret->mTextPages),
    std::static_pointer_cast<IPageSource>(ret->mImagePages),
  });
  co_return ret;
}

DCSBriefingTab::~DCSBriefingTab() { this->RemoveAllEventListeners(); }

std::string DCSBriefingTab::GetGlyph() const { return GetStaticGlyph(); }

std::string DCSBriefingTab::GetStaticGlyph() { return "\uE95D"; }

task<void> DCSBriefingTab::Reload() noexcept {
  const scope_exit emitEvents([this]() {
    this->evContentChangedEvent.Emit();
    this->evAvailableFeaturesChangedEvent.Emit();
    this->evNeedsRepaintEvent.Emit();
  });
  mImagePages->SetPaths({});
  mTextPages->ClearText();

  if (!mMission) {
    co_return;
  }

  const auto root = mMission->GetExtractedPath();
  if (!std::filesystem::exists(root / "mission")) {
    co_return;
  }

  const auto localized = root / "l10n" / "DEFAULT";

  LuaState lua;
  lua.DoFile(root / "mission");
  if (std::filesystem::exists(localized / "dictionary")) {
    lua.DoFile(localized / "dictionary");
  }
  if (std::filesystem::exists(localized / "mapResource")) {
    lua.DoFile(localized / "MapResource");
  }

  const auto mission = lua.GetGlobal("mission");
  const auto dictionary = lua.GetGlobal("dictionary");
  const auto mapResource = lua.GetGlobal("mapResource");

  std::vector<std::pair<std::string_view, std::function<void()>>>
    sectionLoaders {
      {
        "SetMissionImages",
        std::bind_front(
          &DCSBriefingTab::SetMissionImages,
          this,
          mission,
          mapResource,
          localized),
      },
      {
        "PushMissionOverview",
        std::bind_front(
          &DCSBriefingTab::PushMissionOverview, this, mission, dictionary),
      },
      {
        "PushMissionSituation",
        std::bind_front(
          &DCSBriefingTab::PushMissionSituation, this, mission, dictionary),
      },
      {
        "PushMissionObjective",
        std::bind_front(
          &DCSBriefingTab::PushMissionObjective, this, mission, dictionary),
      },
      {
        "PushMissionWeather",
        std::bind_front(&DCSBriefingTab::PushMissionWeather, this, mission),
      },
      {
        "PushBullseyeData",
        std::bind_front(&DCSBriefingTab::PushBullseyeData, this, mission),
      },
    };

  for (const auto& [name, loader]: sectionLoaders) {
    try {
      loader();
    } catch (const LuaIndexError& e) {
      dprint("LuaIndexError in {}: {}", name, e.what());
    } catch (const LuaTypeError& e) {
      dprint("LuaTypeError in {}: {}", name, e.what());
    } catch (const LuaError& e) {
      dprint("LuaError in {}: {}", name, e.what());
    }
  }

  this->evContentChangedEvent.Emit();
}

OpenKneeboard::fire_and_forget DCSBriefingTab::OnAPIEvent(
  APIEvent event,
  std::filesystem::path installPath,
  std::filesystem::path) {
  mInstallationPath = installPath;
  if (event.name == DCSEvents::EVT_MISSION) {
    const auto missionZip = this->ToAbsolutePath(event.value);
    if (missionZip.empty() || !std::filesystem::exists(missionZip)) {
      dprint("Briefing tab: mission '{}' does not exist", event.value);
      co_return;
    }

    if (mMission && mMission->GetZipPath() == missionZip) {
      co_return;
    }

    // Stop watching folders before potentially cleaning
    // up the old extraction folder
    mImagePages->SetPaths({});
    mMission = DCSExtractedMission::Get(missionZip);
    dprint("Briefing tab: loading {}", missionZip);
    co_await this->Reload();
  }

  auto state = mDCSState;
  if (event.name == DCSEvents::EVT_SELF_DATA) {
    auto raw = nlohmann::json::parse(event.value);
    state.mCoalition = static_cast<DCSEvents::Coalition>(
      raw.at("CoalitionID")
        .get<std::underlying_type_t<DCSEvents::Coalition>>()),
    state.mCountry = raw.at("Country");
    state.mAircraft = raw.at("Name");
  } else if (event.name == DCSEvents::EVT_ORIGIN) {
    auto raw = nlohmann::json::parse(event.value);
    state.mOrigin = LatLong {
      .mLat = raw["latitude"],
      .mLong = raw["longitude"],
    };
  } else {
    co_return;
  }

  if (state != mDCSState) {
    mDCSState = state;
    co_await this->Reload();
  }
}

bool DCSBriefingTab::IsNavigationAvailable() const {
  return this->GetPageCount() > 2;
}

std::vector<NavigationEntry> DCSBriefingTab::GetNavigationEntries() const {
  std::vector<NavigationEntry> entries;

  const auto textCount = mTextPages->GetPageCount();
  const auto textIDs = mTextPages->GetPageIDs();
  for (PageIndex i = 0; i < textCount; ++i) {
    entries.push_back({
      std::format(_("Transcription {}/{}"), i + 1, textCount),
      textIDs.at(i),
    });
  }

  const auto paths = mImagePages->GetPaths();
  const auto imageIDs = mImagePages->GetPageIDs();

  for (PageIndex i = 0; i < paths.size(); ++i) {
    entries.push_back({
      to_utf8(paths.at(i).stem()),
      imageIDs.at(i),
    });
  }

  return entries;
}
}// namespace OpenKneeboard
