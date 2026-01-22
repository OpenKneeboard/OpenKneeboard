// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <magic_enum/magic_enum.hpp>

#include <format>

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
    const auto value = std::format(
      "{}({})",
      magic_enum::enum_contains(v) ? magic_enum::enum_name(v) : "[invalid]",
      std::to_underlying(v));
    if (!mIncludeTypeName) {
      return std::copy(value.begin(), value.end(), ctx.out());
    }

    const auto combined =
      std::format("{}::{}", magic_enum::enum_type_name<T>(), value);
    return std::copy(combined.begin(), combined.end(), ctx.out());
  }

 private:
  bool mIncludeTypeName = false;
  static constexpr std::string_view IncludeTypeNameArg {"#"};
};
