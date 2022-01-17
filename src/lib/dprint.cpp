#include <OpenKneeboard/dprint.h>
#include <Windows.h>

namespace OpenKneeboard {

static DPrintSettings gSettings;

void dprint(std::string_view message) {
  auto target = gSettings.target;

  if (target == DPrintSettings::Target::DEFAULT) {
#ifdef NDEBUG
    target = DPrintSettings::Target::DEBUG_STREAM;
#else
    target = DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM;
#endif;
  }

  if (
    target == DPrintSettings::Target::DEBUG_STREAM
    || target == DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM) {
    auto output = fmt::format("[{}] {}\n", gSettings.prefix, message);
    OutputDebugStringA(output.c_str());
  }

  if (
    target == DPrintSettings::Target::CONSOLE
    || target == DPrintSettings::Target::CONSOLE_AND_DEBUG_STREAM) {
    auto output = fmt::format("{}\n", message);
    if (AllocConsole()) {
      // Gets detour trace working too :)
      freopen("CONIN$", "r", stdin);
      freopen("CONOUT$", "w", stdout);
      freopen("CONOUT$", "w", stderr);
    }
    auto handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
      return;
    }
    WriteConsoleA(
      handle,
      output.c_str(),
      static_cast<DWORD>(output.size()),
      nullptr,
      nullptr);
  }
}

void DPrintSettings::Set(const DPrintSettings& settings) {
  gSettings = settings;
}

}// namespace OpenKneeboard
