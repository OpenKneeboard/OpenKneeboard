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
#include <OpenKneeboard/json.hpp>

using namespace OpenKneeboard::detail::SparseJson;

namespace OpenKneeboard {

static_assert(
  ConstStrLowerFirst(Wrap("FooBar")).mBuffer == std::string_view {"fooBar"});
static_assert(
  ConstStrSkipFirst(Wrap("mFooBar")).mBuffer == std::string_view {"FooBar"});
static_assert(
  ConstStrSkipFirstLowerNext(Wrap("mFooBar")).mBuffer
  == std::string_view {"fooBar"});

}// namespace OpenKneeboard

namespace nlohmann {
void adl_serializer<winrt::guid>::to_json(json& j, const winrt::guid& v) {
  j = winrt::to_string(winrt::to_hstring(v));
}
void adl_serializer<winrt::guid>::from_json(const json& j, winrt::guid& v) {
  v = winrt::guid {j.get<std::string>()};
}
}// namespace nlohmann
