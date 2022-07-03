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

#include <OpenKneeboard/DCSBriefingTab.h>
#include <OpenKneeboard/DCSExtractedMission.h>
#include <OpenKneeboard/DCSWorld.h>
#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/ImagePageSource.h>
#include <OpenKneeboard/PlainTextPageSource.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/scope_guard.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <LuaBridge/LuaBridge.h>

using DCS = OpenKneeboard::DCSWorld;

namespace OpenKneeboard {

DCSBriefingTab::DCSBriefingTab(const DXResources& dxr, KneeboardState* kbs)
  : TabWithDoodles(dxr, kbs),
    mImagePages(std::make_unique<ImagePageSource>(dxr)),
    mTextPages(std::make_unique<PlainTextPageSource>(dxr, _("[no briefing]"))) {
}

DCSBriefingTab::~DCSBriefingTab() {
  this->RemoveAllEventListeners();
}

utf8_string DCSBriefingTab::GetGlyph() const {
  return {};// FIXME
}

utf8_string DCSBriefingTab::GetTitle() const {
  return _("Briefing");
}

uint16_t DCSBriefingTab::GetPageCount() const {
  return mImagePages->GetPageCount() + mTextPages->GetPageCount();
}

D2D1_SIZE_U DCSBriefingTab::GetNativeContentSize(uint16_t pageIndex) {
  const auto imageCount = mImagePages->GetPageCount();
  if (pageIndex < imageCount) {
    return mImagePages->GetNativeContentSize(pageIndex);
  }
  return mTextPages->GetNativeContentSize(pageIndex - imageCount);
}

void DCSBriefingTab::Reload() noexcept {
  const scope_guard emitEvents([this]() {
    this->evFullyReplacedEvent.Emit();
    this->evNeedsRepaintEvent.Emit();
  });
  mImagePages->SetPaths({});
  mTextPages->ClearText();

  lua_State* lua = lua_open();
  scope_guard closeLua([&lua]() { lua_close(lua); });

  luaL_openlibs(lua);

  const auto root = mMission->GetExtractedPath();

  int error = luaL_dofile(lua, to_utf8(root / "mission").c_str());
  if (error) {
    dprintf("Failed to load lua mission table: {}", lua_tostring(lua, -1));
    return;
  }

  const auto localized = root / "l10n" / "DEFAULT";
  error = luaL_dofile(lua, to_utf8(localized / "dictionary").c_str());
  if (error) {
    dprintf("Failed to load lua dictionary table: {}", lua_tostring(lua, -1));
    return;
  }

  const auto mission = luabridge::getGlobal(lua, "mission");
  const auto dictionary = luabridge::getGlobal(lua, "dictionary");

  // WIP FIXME
  constexpr bool isRedFor = true;

  if (std::filesystem::exists(localized / "mapResource")) {
    error = luaL_dofile(lua, to_utf8(localized / "MapResource").c_str());
    if (error) {
      dprintf("Failed to load lua mapResource: {}", lua_tostring(lua, -1));
      return;
    }

    const auto mapResource = luabridge::getGlobal(lua, "mapResource");

    std::vector<std::filesystem::path> images;

    luabridge::LuaRef force
      = mission[isRedFor ? "pictureFileNameR" : "pictureFileNameB"];
    for (auto&& [i, resourceName]: luabridge::pairs(force)) {
      const auto fileName = mapResource[resourceName].cast<std::string>();
      const auto path = localized / fileName;
      if (std::filesystem::is_regular_file(path)) {
        images.push_back(path);
      }
    }
    mImagePages->SetPaths(images);
  }

  const std::string title = dictionary[mission["sortie"]];

  mTextPages->SetText(title);
}

void DCSBriefingTab::OnGameEvent(
  const GameEvent& event,
  const std::filesystem::path&,
  const std::filesystem::path&) {
  if (event.name != DCS::EVT_MISSION) {
    return;
  }

  const auto missionZip = std::filesystem::canonical(event.value);

  if (mMission && mMission->GetZipPath() == missionZip) {
    return;
  }

  mMission = DCSExtractedMission::Get(missionZip);
  dprintf("Briefing tab: loading {}", missionZip);
  this->Reload();
}

void DCSBriefingTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t pageIndex,
  const D2D1_RECT_F& rect) {
  const auto imageCount = mImagePages->GetPageCount();
  if (pageIndex < imageCount) {
    mImagePages->RenderPage(ctx, pageIndex, rect);
    return;
  }
  return mTextPages->RenderPage(ctx, pageIndex - imageCount, rect);
}

}// namespace OpenKneeboard
