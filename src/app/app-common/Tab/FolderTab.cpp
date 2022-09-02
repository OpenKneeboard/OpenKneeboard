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
#include <OpenKneeboard/FolderPageSource.h>
#include <OpenKneeboard/FolderTab.h>
#include <OpenKneeboard/dprint.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

FolderTab::FolderTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : PageSourceWithDelegates(dxr, kbs),
    mPageSource(std::make_shared<FolderPageSource>(dxr, kbs, path)),
    mPath {path} {
  this->SetDelegates({mPageSource});
  this->Reload();
}

FolderTab::FolderTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view title,
  const nlohmann::json& settings)
  : FolderTab(
    dxr,
    kbs,
    title,
    settings.at("Path").get<std::filesystem::path>()) {
}

FolderTab::~FolderTab() {
}

nlohmann::json FolderTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

utf8_string FolderTab::GetGlyph() const {
  return "\uE838";
}

utf8_string FolderTab::GetTitle() const {
  return mPath.filename();
}

void FolderTab::Reload() {
  mPageSource->Reload();
}

std::filesystem::path FolderTab::GetPath() const {
  return mPath;
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  if (path == mPath) {
    return;
  }
  mPageSource->SetPath(path);
  mPath = path;
}

}// namespace OpenKneeboard
