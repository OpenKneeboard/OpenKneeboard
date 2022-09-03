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
#include <OpenKneeboard/FilePageSource.h>
#include <OpenKneeboard/ImageFilePageSource.h>
#include <OpenKneeboard/PDFFilePageSource.h>
#include <OpenKneeboard/PlainTextFilePageSource.h>
#include <OpenKneeboard/SingleFileTab.h>
#include <OpenKneeboard/scope_guard.h>

#include <nlohmann/json.hpp>

namespace OpenKneeboard {

SingleFileTab::SingleFileTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : PageSourceWithDelegates(dxr, kbs), mDXR(dxr), mKneeboard(kbs) {
  this->SetPath(path);
}

SingleFileTab::SingleFileTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view title,
  const nlohmann::json& settings)
  : SingleFileTab(
    dxr,
    kbs,
    title,
    settings.at("Path").get<std::filesystem::path>()) {
}

SingleFileTab::~SingleFileTab() {
}

nlohmann::json SingleFileTab::GetSettings() const {
  return {{"Path", GetPath()}};
}

utf8_string SingleFileTab::GetGlyph() const {
  switch (mKind) {
    case Kind::PDFFile:
      return "\uEA90";
    case Kind::ImageFile:
      return "\uE91B";
    case Kind::PlainTextFile:
      return "\uE8A5";
    default:
      return "";
  }
}

utf8_string SingleFileTab::GetTitle() const {
  return mPath.stem();
}

std::filesystem::path SingleFileTab::GetPath() const {
  return mPath;
}

void SingleFileTab::SetPath(const std::filesystem::path& rawPath) {
  auto path = rawPath;
  if (std::filesystem::exists(path)) {
    path = std::filesystem::canonical(path);
  }
  if (path == mPath) {
    return;
  }
  mPath = path;

  this->Reload();
}

void SingleFileTab::Reload() {
  mKind = Kind::Unknown;
  auto delegate = FilePageSource::Get(mDXR, mKneeboard, mPath);
  if (!delegate) {
    this->SetDelegates({});
  }

  if (std::dynamic_pointer_cast<PDFFilePageSource>(delegate)) {
    mKind = Kind::PDFFile;
  } else if (std::dynamic_pointer_cast<PlainTextFilePageSource>(delegate)) {
    mKind = Kind::PlainTextFile;
  } else if (std::dynamic_pointer_cast<ImageFilePageSource>(delegate)) {
    mKind = Kind::ImageFile;
  }

  this->SetDelegates({delegate});
}

}// namespace OpenKneeboard
