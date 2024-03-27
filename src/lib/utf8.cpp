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

#include <OpenKneeboard/utf8.h>

#include <shims/winrt/base.h>

#include <regex>

#include <icu.h>

namespace OpenKneeboard {

std::string to_utf8(const std::filesystem::path& in) {
  const auto yaycpp20 = in.u8string();
  return {reinterpret_cast<const char*>(yaycpp20.data()), yaycpp20.size()};
}

std::string to_utf8(const std::wstring& in) {
  return winrt::to_string(in);
}

std::string to_utf8(std::wstring_view in) {
  return winrt::to_string(in);
}

std::string to_utf8(const wchar_t* in) {
  return to_utf8(std::wstring_view(in));
}

class UTF8CaseMap {
 public:
  UTF8CaseMap(const char locale[]) {
    auto error = U_ZERO_ERROR;
    mImpl = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &error);
  }

  ~UTF8CaseMap() {
    if (!mImpl) {
      return;
    }

    ucasemap_close(mImpl);
  }

  inline operator const UCaseMap*() const noexcept {
    return mImpl;
  }

  UTF8CaseMap() = delete;
  UTF8CaseMap(const UTF8CaseMap&) = delete;
  UTF8CaseMap& operator=(const UTF8CaseMap&) = delete;

 private:
  UCaseMap* mImpl {nullptr};
};

static UTF8CaseMap gFoldCaseMap("");

std::string fold_utf8(std::string_view in) {
  auto error = U_ZERO_ERROR;
  const auto foldedLength = ucasemap_utf8FoldCase(
    gFoldCaseMap,
    nullptr,
    0,
    in.data(),
    static_cast<int32_t>(in.size()),
    &error);

  std::string ret(static_cast<size_t>(foldedLength), '\0');
  error = U_ZERO_ERROR;
  const auto neededLength = ucasemap_utf8FoldCase(
    gFoldCaseMap,
    ret.data(),
    ret.size(),
    in.data(),
    static_cast<int32_t>(in.size()),
    &error);
  return ret;
}

}// namespace OpenKneeboard

NLOHMANN_JSON_NAMESPACE_BEGIN

void adl_serializer<std::filesystem::path>::from_json(
  const nlohmann::json& j,
  std::filesystem::path& p) {
  const auto utf8 = j.get<std::string>();
  const auto first = reinterpret_cast<const char8_t*>(utf8.data());

  p = std::filesystem::path {first, first + utf8.size()};
  // forward slashes to backslashes
  p.make_preferred();
}
void adl_serializer<std::filesystem::path>::to_json(
  nlohmann::json& j,
  const std::filesystem::path& p) {
  j = OpenKneeboard::to_utf8(p);
}

NLOHMANN_JSON_NAMESPACE_END
