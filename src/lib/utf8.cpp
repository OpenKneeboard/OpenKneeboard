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
#pragma once

#include <OpenKneeboard/utf8.h>
#include <shims/winrt.h>

namespace OpenKneeboard {

std::string to_utf8(const std::filesystem::path& in) {
  return winrt::to_string(in.wstring());
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

utf8_string_view::operator std::filesystem::path() const {
  return std::filesystem::path(std::wstring_view(winrt::to_hstring(*this)));
}

}// namespace OpenKneeboard

namespace std::filesystem {
void from_json(const nlohmann::json& j, std::filesystem::path& p) {
  p = static_cast<std::wstring_view>(winrt::to_hstring(j.get<std::string>()));
}
void to_json(nlohmann::json& j, const std::filesystem::path& p) {
  j = winrt::to_string(p.wstring());
}
}// namespace std::filesystem
