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
#include <OpenKneeboard/final_release_deleter.h>
#include <OpenKneeboard/json.h>
#include <OpenKneeboard/scope_guard.h>
#include <Windows.h>

#include <thread>

namespace OpenKneeboard {

std::shared_ptr<GameEventServer> GameEventServer::Create() {
  auto ret = shared_with_final_release(new GameEventServer());
  ret->Start();
  return ret;
}

winrt::fire_and_forget GameEventServer::final_release(
  std::unique_ptr<GameEventServer> self) {
  self->mRunner.Cancel();
  co_await winrt::resume_on_signal(self->mCompletionHandle.get());
}

GameEventServer::GameEventServer() {
  dprintf("{}", __FUNCTION__);
}

void GameEventServer::Start() {
  mRunner = this->Run();
}

GameEventServer::~GameEventServer() {
  dprintf("{}", __FUNCTION__);
}

winrt::Windows::Foundation::IAsyncAction GameEventServer::Run() {
  const scope_guard markCompletion(
    [handle = mCompletionHandle.get()]() { SetEvent(handle); });

  auto weak = weak_from_this();
  winrt::file_handle handle {CreateMailslotA(
    GameEvent::GetMailslotPath(), 0, MAILSLOT_WAIT_FOREVER, nullptr)};
  if (!handle) {
    dprintf("Failed to create GameEvent mailslot: {}", GetLastError());
    co_return;
  }

  dprint("Started listening for game events");
  const scope_guard logOnExit([]() {
    dprintf(
      "GameEventServer shutting down with {} uncaught exceptions",
      std::uncaught_exceptions());
  });

  auto cancelled = co_await winrt::get_cancellation_token();
  cancelled.enable_propagation();

  winrt::handle event {CreateEventW(nullptr, FALSE, FALSE, nullptr)};

  try {
    while ((!cancelled()) && co_await RunSingle(weak, event, handle)) {
      // repeat!
    }
  } catch (const winrt::hresult_canceled&) {
    co_return;
  }

  co_return;
}

winrt::Windows::Foundation::IAsyncOperation<bool> GameEventServer::RunSingle(
  const std::weak_ptr<GameEventServer>& instance,
  const winrt::handle& notifyEvent,
  const winrt::file_handle& handle) {
  auto cancelled = co_await winrt::get_cancellation_token();
  cancelled.enable_propagation();

  OVERLAPPED overlapped {
    .hEvent = notifyEvent.get(),
  };

  char buffer[4096];
  DWORD bytesRead {};
  const auto readFileResult = ReadFile(
    handle.get(),
    buffer,
    static_cast<DWORD>(std::size(buffer)),
    &bytesRead,
    &overlapped);
  const auto readFileError = GetLastError();
  if ((!readFileResult) && readFileError != ERROR_IO_PENDING) {
    dprintf("GameEvent ReadFile failed: {}", GetLastError());
    co_return true;
  }

  co_await winrt::resume_on_signal(notifyEvent.get());
  GetOverlappedResult(handle.get(), &overlapped, &bytesRead, TRUE);

  if (bytesRead == 0) {
    dprint("Read 0-byte GameEvent message");
    co_return true;
  }

  auto self = instance.lock();
  if (!self) {
    co_return false;
  }

  auto event = GameEvent::Unserialize({buffer, bytesRead});
  if (event.name != GameEvent::EVT_MULTI_EVENT) {
    self->evGameEvent.Emit(event);
    co_return true;
  }

  std::vector<std::tuple<std::string, std::string>> events;
  events = nlohmann::json::parse(event.value);
  for (const auto& [name, value]: events) {
    self->evGameEvent.Emit({name, value});
  }

  co_return true;
}

}// namespace OpenKneeboard
