#include "okGameEventNamedPipeThread.h"

#include <OpenKneeboard/dprint.h>
#include <Windows.h>

#include <charconv>
#include <string_view>

using OpenKneeboard::dprint;
using OpenKneeboard::dprintf;

uint32_t hex_to_ui32(const std::string_view& sv) {
  if (sv.empty()) {
    return 0;
  }

  uint32_t value = 0;
  std::from_chars(&sv.front(), &sv.front() + sv.length(), value, 16);
  return value;
}

wxDEFINE_EVENT(okEVT_GAME_EVENT, wxThreadEvent);

#define CHECK_PACKET(condition) \
  if (!(condition)) { \
    dprintf("Check failed at {}:{}: {}", __FILE__, __LINE__, #condition); \
    continue; \
  }

okGameEventNamedPipeThread::okGameEventNamedPipeThread(wxFrame* parent)
  : wxThread(), mParent(parent) {
}

okGameEventNamedPipeThread::~okGameEventNamedPipeThread() {
}

wxThread::ExitCode okGameEventNamedPipeThread::Entry() {
  char buffer[1024];
  HANDLE pipe = CreateNamedPipeA(
    "\\\\.\\pipe\\com.fredemmott.openkneeboard.events.v1",
    PIPE_ACCESS_INBOUND,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
    PIPE_UNLIMITED_INSTANCES,
    sizeof(buffer),
    0,
    10000,
    nullptr);
  if (!pipe) {
    dprint("No pipe!");
    return ExitCode(1);
  }

  dprint("Started listening for game events.");

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

    // "{:08x}!{}!{:08x}!{}!", name size, name, value size, value
    const std::string packet(buffer, bytesRead);
    CHECK_PACKET(packet.ends_with("!"));
    CHECK_PACKET(packet.size() >= sizeof("12345678!!12345678!!") - 1);

    const std::string_view pv(packet);

    const auto nameLen = hex_to_ui32(pv.substr(0, 8));
    CHECK_PACKET(packet.size() >= 8 + nameLen + 8 + 4);
    const uint32_t nameOffset = 9;
    const auto name = packet.substr(nameOffset, nameLen);

    const uint32_t valueLenOffset = nameOffset + nameLen + 1;
    CHECK_PACKET(packet.size() >= valueLenOffset + 10);
    const auto valueLen = hex_to_ui32(pv.substr(valueLenOffset, 8));
    const uint32_t valueOffset = valueLenOffset + 8 + 1;
    CHECK_PACKET(packet.size() == valueOffset + valueLen + 1);
    const auto value = packet.substr(valueOffset, valueLen);

    dprintf("Game Event: {}\n  {}", name, value);

    auto ev = new wxThreadEvent(okEVT_GAME_EVENT);
    ev->SetPayload(Payload {.Name = name, .Value = value});
    wxQueueEvent(mParent, ev);
  }

  return ExitCode(0);
}
