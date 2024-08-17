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

#include <format>

#include <magic_enum.hpp>

template <class T>
  requires std::is_scoped_enum_v<T>
struct std::formatter<T, char> {
  constexpr auto parse(auto& ctx) {
    const auto end = ctx.end();
    auto it = begin(ctx);
    while (it != end && *it != '}') {
      if (std::string_view(it, end).starts_with(IncludeTypeNameArg)) {
        mIncludeTypeName = true;
        ++it;
        continue;
      }
      throw std::format_error("Invalid format arg for enum");
    }
    return it;
  }

  constexpr auto format(T v, auto& ctx) const {
    const auto value
      = std::format("{}({})", magic_enum::enum_name(v), std::to_underlying(v));
    if (!mIncludeTypeName) {
      return std::copy(value.begin(), value.end(), ctx.out());
    }

    const auto combined
      = std::format("{}::{}", magic_enum::enum_type_name<T>(), value);
    return std::copy(combined.begin(), combined.end(), ctx.out());
  }

 private:
  bool mIncludeTypeName = false;
  static constexpr std::string_view IncludeTypeNameArg {"#"};
};