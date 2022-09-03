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
#include <OpenKneeboard/PDFFilePageSource.h>
#include <OpenKneeboard/PDFTab.h>

namespace OpenKneeboard {

PDFTab::PDFTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : PageSourceWithDelegates(dxr, kbs) {
  mPageSource = std::make_shared<PDFFilePageSource>(dxr, kbs, path);
  this->SetDelegates({mPageSource});
}

PDFTab::PDFTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view title,
  const nlohmann::json& settings)
  : PDFTab(dxr, kbs, title, settings.at("Path").get<std::filesystem::path>()) {
}

PDFTab::~PDFTab() {
  this->RemoveAllEventListeners();
}

nlohmann::json PDFTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

utf8_string PDFTab::GetGlyph() const {
  return "\uEA90";
}

utf8_string PDFTab::GetTitle() const {
  return mPageSource->GetPath().stem();
}

void PDFTab::Reload() {
  mPageSource->Reload();
}

std::filesystem::path PDFTab::GetPath() const {
  return mPageSource->GetPath();
}

void PDFTab::SetPath(const std::filesystem::path& path) {
  mPageSource->SetPath(path);
}

}// namespace OpenKneeboard
