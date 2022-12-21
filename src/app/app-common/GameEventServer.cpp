/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#include <OpenKneeboard/GameEventServer.h>
#include <OpenKneeboard/config.h>
#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/json.h>
#include <OpenKneeboard/scope_guard.h>
#include <Windows.h>

#include <thread>

namespace OpenKneeboard {

winrt::Windows::Foundation::IAsyncAction GameEventServer::Run() {
  winrt::file_handle handle {CreateMailslotA(
    GameEvent::GetMailslotPath(), 0, MAILSLOT_WAIT_FOREVER, nullptr)};
  if (!handle) {
    dprint("Failed to create GameEvent mailslot");
    co_return;
  }

  dprint("Started listening for game events");
  const scope_guard logOnExit([]() {
    dprintf(
      "GameEventServer shutting down with {} uncaught exceptions",
      std::uncaught_exceptions());
  });

  auto cancelled = co_await winrt::get_cancellation_token();

  while (!cancelled()) {
    DWORD unreadMessageCount = 0;
    DWORD nextMessageSize = 0;
    GetMailslotInfo(
      handle.get(), nullptr, &nextMessageSize, &unreadMessageCount, nullptr);
    if (unreadMessageCount == 0) {
      co_await winrt::resume_after(std::chrono::milliseconds(100));
      continue;
    }

    std::vector<std::byte> buffer(nextMessageSize);
    DWORD bytesRead;
    if (!ReadFile(
          handle.get(),
          reinterpret_cast<void*>(buffer.data()),
          static_cast<DWORD>(buffer.size()),
          &bytesRead,
          nullptr)) {
      dprintf("GameEvent ReadFile failed: {}", GetLastError());
      continue;
    }

    if (bytesRead == 0) {
      continue;
    }

    auto event = GameEvent::Unserialize(buffer);
    if (event.name != GameEvent::EVT_MULTI_EVENT) {
      evGameEvent.Emit(event);
      continue;
    }

    std::vector<std::tuple<std::string, std::string>> events;
    events = nlohmann::json::parse(event.value);
    for (const auto& [name, value]: events) {
      evGameEvent.Emit({name, value});
    }
  }

  co_return;
}

}// namespace OpenKneeboard
