#include "okGameEventMailslotThread.h"

#include <Windows.h>
#include <winrt/base.h>

#include <charconv>
#include <string_view>

#include "OpenKneeboard/dprint.h"

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
    CHECK_PACKET(bytesRead == buffer.size());

    if (bytesRead == 0) {
      continue;
    }

    // "{:08x}!{}!{:08x}!{}!", name size, name, value size, value
    const std::string_view packet(
      reinterpret_cast<const char*>(buffer.data()), buffer.size());
    CHECK_PACKET(packet.ends_with("!"));
    CHECK_PACKET(packet.size() >= sizeof("12345678!!12345678!!") - 1);

    const auto nameLen = hex_to_ui32(packet.substr(0, 8));
    CHECK_PACKET(packet.size() >= 8 + nameLen + 8 + 4);
    const uint32_t nameOffset = 9;
    const auto name = packet.substr(nameOffset, nameLen);

    const uint32_t valueLenOffset = nameOffset + nameLen + 1;
    CHECK_PACKET(packet.size() >= valueLenOffset + 10);
    const auto valueLen = hex_to_ui32(packet.substr(valueLenOffset, 8));
    const uint32_t valueOffset = valueLenOffset + 8 + 1;
    CHECK_PACKET(packet.size() == valueOffset + valueLen + 1);
    const auto value = packet.substr(valueOffset, valueLen);

    dprintf("Game Event: {}\n  {}", name, value);

    wxThreadEvent ev(okEVT_GAME_EVENT);
    ev.SetPayload(
      Payload {.Name = std::string(name), .Value = std::string(value)});
    wxQueueEvent(mParent, ev.Clone());
  }

  return ExitCode(0);
}
