#include "okGameInjectorThread.h"

#include <TlHelp32.h>
#include <winrt/base.h>
#include <detours.h>

#include "OpenKneeboard/dprint.h"

#include "Injectables.h"

using namespace OpenKneeboard;

class okGameInjectorThread::Impl final {
 public:
  std::vector<GameInstance> Games;
};

okGameInjectorThread::okGameInjectorThread(
  const std::vector<GameInstance>& games) {
  p.reset(new Impl {
    .Games = games,
  });
}

okGameInjectorThread::~okGameInjectorThread() {
}

wxThread::ExitCode okGameInjectorThread::Entry() {
  PROCESSENTRY32 process;
  process.dwSize = sizeof(process);
  MODULEENTRY32 module;
  module.dwSize = sizeof(module);
  dprint("Looking for game processes...");
  while (IsAlive()) {
    winrt::handle snapshot {
      CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
      dprintf("CreateToolhelp32Snapshot failed in {}", __FILE__);
      return ExitCode(1);
    }

    dprint("Got game snapshot");

    if (!Process32First(snapshot.get(), &process)) {
      dprintf("Process32First failed: {}", GetLastError());
      return ExitCode(2);
    }

    do {
      winrt::handle processHandle { OpenProcess(PROCESS_ALL_ACCESS, false, process.th32ProcessID) };
      if (!processHandle) {
        continue;
      }
      wchar_t buf[MAX_PATH];
      DWORD bufSize = sizeof(buf);
      QueryFullProcessImageNameW(processHandle.get(), 0, buf, &bufSize);
      const std::filesystem::path path(std::wstring(buf, bufSize));
      for (const auto& game: p->Games) {
        if (path == game.Path) {
          auto friendly = game.Game->GetUserFriendlyName(game.Path);

          dprintf(
            "Found '{}' process: {} {}",
            friendly.ToStdString(),
            process.th32ProcessID,
            path.string());
          // TODO: check for installed location
          const char* dllPath = Injectables::Oculus_D3D11.BuildPath;
          if (DetourUpdateProcessWithDll(processHandle.get(), &dllPath, 1)) {
            dprint("Injected DLL.");
          } else {
            dprintf("Failed to inject DLL: {}", GetLastError());
          }
        }
      }
    } while (Process32Next(snapshot.get(), &process));

    wxThread::This()->Sleep(1000);
  }
  return ExitCode(0);
}
