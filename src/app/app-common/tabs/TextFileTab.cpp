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
#include <OpenKneeboard/PlainTextPageSource.h>
#include <OpenKneeboard/TextFileTab.h>
#include <OpenKneeboard/scope_guard.h>

#include <fstream>
#include <nlohmann/json.hpp>

namespace OpenKneeboard {

TextFileTab::TextFileTab(
  const DXResources& dxr,
  KneeboardState* kbs,
  utf8_string_view /* title */,
  const std::filesystem::path& path)
  : TabWithDoodles(dxr, kbs),
    mPath(path),
    mPageSource(std::make_unique<PlainTextPageSource>(dxr, _("[empty file]"))) {
  Reload();
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
  return mPath.stem();
}

void TextFileTab::RenderPageContent(
  ID2D1DeviceContext* ctx,
  uint16_t index,
  const D2D1_RECT_F& rect) {
  mPageSource->RenderPage(ctx, index, rect);
}

std::filesystem::path TextFileTab::GetPath() const {
  return mPath;
}

void TextFileTab::SetPath(const std::filesystem::path& path) {
  if (path == mPath) {
    return;
  }
  mPath = path;
  Reload();
}

void TextFileTab::Reload() {
  this->ClearContentCache();
  scope_guard emitEvents([this]() {
    this->evFullyReplacedEvent.Emit();
    this->evNeedsRepaintEvent.Emit();
  });

  if (!std::filesystem::is_regular_file(mPath)) {
    mPageSource->ClearText();
    return;
  }

  auto bytes = std::filesystem::file_size(mPath);
  std::string buffer;
  buffer.resize(bytes);
  size_t offset = 0;

  std::ifstream f(mPath, std::ios::in | std::ios::binary);
  if (!f.is_open()) {
    mPageSource->ClearText();
    return;
  }
  while (bytes > 0) {
    f.read(&buffer.data()[offset], bytes);
    bytes -= f.gcount();
    offset += f.gcount();
  }

  size_t pos = 0;
  while ((pos = buffer.find("\r\n", pos)) != std::string::npos) {
    buffer.replace(pos, 2, "\n");
    pos++;
  }

  mPageSource->SetText(buffer);
}

uint16_t TextFileTab::GetPageCount() const {
  return mPageSource->GetPageCount();
}

D2D1_SIZE_U TextFileTab::GetNativeContentSize(uint16_t pageIndex) {
  return mPageSource->GetNativeContentSize(pageIndex);
}

}// namespace OpenKneeboard
