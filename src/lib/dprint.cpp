#include <OpenKneeboard/dprint.h>
#include <Windows.h>

namespace OpenKneeboard {

static DPrintSettings gSettings;

void dprint(const std::string& message) {
  auto target = gSettings.target;
  if (target == DPrintSettings::Target::DEFAULT) {
#ifdef NDEBUG
    target = DPrintSettings::Target::DEBUG_STREAM;
#else
    if (IsDebuggerPresent()) {
      target = DPrintSettings::Target::DEBUG_STREAM;
    } else {
      target = DPrintSettings::Target::CONSOLE;
    }
#endif;
  }

  if (target == DPrintSettings::Target::DEBUG_STREAM) {
    auto output = fmt::format("[{}] {}\n", gSettings.prefix, message);
    OutputDebugStringA(output.c_str());
    return;
  }

  if (target == DPrintSettings::Target::CONSOLE) {
    auto output = fmt::format("{}\n", message);
    AllocConsole();
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
    return;
  }

  DebugBreak();
}

void DPrintSettings::Set(const DPrintSettings& settings) {
  gSettings = settings;
}

}// namespace OpenKneeboard
