#pragma once

#include <fmt/format.h>

namespace OpenKneeboard {

void dprint(std::string_view s);

template <typename... T>
void dprintf(fmt::format_string<T...> fmt, T&&... args) {
  dprint(fmt::format(fmt, std::forward<T>(args)...));
}

struct DPrintSettings {
  enum class Target {
    // Traditional console window, created if necessary
    CONSOLE,
    // Use debugger or `dbgview` etc to view this
    DEBUG_STREAM,
    CONSOLE_AND_DEBUG_STREAM,
    // DEBUG_STREAM if release build, CONSOLE_AND_DEBUG_STREAM otherwise
    DEFAULT
  };

  std::string prefix = "OpenKneeboard";
  Target target = Target::DEFAULT;

  static void Set(const DPrintSettings&);
};

}// namespace OpenKneeboard
