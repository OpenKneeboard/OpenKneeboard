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

}// namespace OpenKneeboard

NLOHMANN_JSON_NAMESPACE_BEGIN

void adl_serializer<std::filesystem::path>::from_json(
  const nlohmann::json& j,
  std::filesystem::path& p) {
  const auto utf8 = j.get<std::string>();
  const auto first = reinterpret_cast<const char8_t*>(utf8.data());
  p = std::filesystem::path {first, first + utf8.size()};
}
void adl_serializer<std::filesystem::path>::to_json(
  nlohmann::json& j,
  const std::filesystem::path& p) {
  j = OpenKneeboard::to_utf8(p);
}

NLOHMANN_JSON_NAMESPACE_END
