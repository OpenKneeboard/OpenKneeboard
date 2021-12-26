#include "EventListener.h"

#include <Windows.h>

#include "OpenKneeboard/dprint.h"

using OpenKneeboard::dprint;

EventListener::EventListener(wxFrame* parent) : wxThread(), mParent(parent) {
}

EventListener::~EventListener() {
}

wxThread::ExitCode EventListener::Entry() {
  char buffer[1024];
  HANDLE pipe = CreateNamedPipeA(
    "\\\\.\\pipe\\com.fredemmott.openkneeboard.events.v1",
    PIPE_ACCESS_INBOUND,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    PIPE_UNLIMITED_INSTANCES,
    sizeof(buffer) - 1,
    0,
    10000,
    nullptr);
  if (!pipe) {
    dprint("No pipe!");
    return ExitCode(1);
  }

  while (IsAlive()) {
    DWORD bytesRead;
    ConnectNamedPipe(pipe, nullptr);
    ReadFile(
          pipe,
          reinterpret_cast<void*>(buffer),
          sizeof(buffer),
          &bytesRead,
          nullptr);
    DisconnectNamedPipe(pipe);

    if (bytesRead == 0) {
      dprint("No bytes");
      continue;
    }

    buffer[bytesRead] = 0;
  }

  return ExitCode(0);
}
