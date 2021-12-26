#pragma once

#include <fmt/format.h>

namespace OpenKneeboard {

void dprint(const std::string& s);

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
  };
  static const Target DEFAULT_TARGET =
#ifdef DEBUG
    Target::CONSOLE;
#else
    Target::DEBUG_STREAM;
#endif

  std::string Prefix = "OpenKneeboard";
  Target Target = DEFAULT_TARGET;

  static void Set(const DPrintSettings&);
};

}// namespace OpenKneeboard
