#include "okGameEventMailslotThread.h"

#include <OpenKneeboard/GameEvent.h>
#include <OpenKneeboard/dprint.h>
#include <Windows.h>
#include <winrt/base.h>

using namespace OpenKneeboard;

wxDEFINE_EVENT(okEVT_GAME_EVENT, wxThreadEvent);

okGameEventMailslotThread::okGameEventMailslotThread(wxFrame* parent)
  : wxThread(), mParent(parent) {
}

okGameEventMailslotThread::~okGameEventMailslotThread() {
}

wxThread::ExitCode okGameEventMailslotThread::Entry() {
  winrt::file_handle handle {CreateMailslotA(
    "\\\\.\\mailslot\\com.fredemmott.openkneeboard.events.v1",
    0,
    MAILSLOT_WAIT_FOREVER,
    nullptr)};
  if (!handle) {
    dprint("Failed to create mailslot");
    return ExitCode(1);
  }

  dprint("Started listening for game events.");

  while (IsAlive()) {
    DWORD unreadMessageCount = 0;
    DWORD nextMessageSize = 0;
    GetMailslotInfo(
      handle.get(), nullptr, &nextMessageSize, &unreadMessageCount, nullptr);
    if (unreadMessageCount == 0) {
      this->Sleep(100);
      continue;
    }
    std::vector<std::byte> buffer(nextMessageSize);
    DWORD bytesRead;
    if (!ReadFile(
          handle.get(),
          reinterpret_cast<void*>(buffer.data()),
          buffer.size(),
          &bytesRead,
          nullptr)) {
      dprintf("GameEvent ReadFile failed: {}", GetLastError());
      continue;
    }

    if (bytesRead == 0) {
      continue;
    }

    auto ge = GameEvent::Unserialize(buffer);

    wxThreadEvent ev(okEVT_GAME_EVENT);
    ev.SetPayload(ge);
    wxQueueEvent(mParent, ev.Clone());
  }

  return ExitCode(0);
}
