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
#include <OpenKneeboard/PlainTextFilePageSource.h>
#include <OpenKneeboard/TextFileTab.h>
#include <OpenKneeboard/scope_guard.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

TextFileTab::TextFileTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : PageSourceWithDelegates(dxr, kbs),
    mPageSource(std::make_shared<PlainTextFilePageSource>(dxr, kbs, path)) {
  this->SetDelegates({std::static_pointer_cast<IPageSource>(mPageSource)});
}

TextFileTab::TextFileTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view title,
  const nlohmann::json& settings)
  : TextFileTab(
    dxr,
    kbs,
    title,
    settings.at("Path").get<std::filesystem::path>()) {
}

TextFileTab::~TextFileTab() {
}

nlohmann::json TextFileTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

utf8_string TextFileTab::GetGlyph() const {
  return "\uE8A5";
}

utf8_string TextFileTab::GetTitle() const {
  return mPageSource->GetPath().stem();
}

std::filesystem::path TextFileTab::GetPath() const {
  return mPageSource->GetPath();
}

void TextFileTab::SetPath(const std::filesystem::path& rawPath) {
  auto path = rawPath;
  if (std::filesystem::exists(path)) {
    path = std::filesystem::canonical(path);
  }

  if (path == this->GetPath()) {
    return;
  }

  mPageSource->SetPath(path);
}

void TextFileTab::Reload() {
  mPageSource->Reload();
}

}// namespace OpenKneeboard
